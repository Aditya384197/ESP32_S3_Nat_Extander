#include "system_metrics.h"

#include "esp_timer.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "driver/temperature_sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#include "nvs.h"

#define NS "router"
#define KEY_PERF "perf_pct"
#define METRICS_CORE 1
#define CORE_COUNT 2

static temperature_sensor_handle_t s_sensor = NULL;
static volatile float s_temp = -1.0f;
static uint8_t s_perf = 100;
static TaskHandle_t s_task = NULL;
static configRUN_TIME_COUNTER_TYPE s_last_idle[CORE_COUNT] = {0, 0};
static int64_t s_last_sample_us = 0;
static bool s_have_baseline = false;
static uint8_t s_cpu[CORE_COUNT] = {0, 0};

static uint8_t load_from_idle(configRUN_TIME_COUNTER_TYPE idle_delta, uint64_t elapsed_us)
{
    if (elapsed_us == 0) return 0;

    uint64_t idle_us = (uint64_t)idle_delta;
    if (idle_us > elapsed_us) idle_us = elapsed_us;

    uint64_t busy = elapsed_us - idle_us;
    uint64_t load = (busy * 100ULL) / elapsed_us;
    return load > 100ULL ? 100U : (uint8_t)load;
}

static void metrics_task(void *arg)
{
    (void)arg;

    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    if (temperature_sensor_install(&cfg, &s_sensor) == ESP_OK) {
        if (temperature_sensor_enable(s_sensor) != ESP_OK) {
            s_sensor = NULL;
        }
    }

    s_last_sample_us = 0;
    s_last_idle[0] = 0;
    s_last_idle[1] = 0;
    s_have_baseline = false;

    for (;;) {
        if (s_sensor != NULL) {
            float temperature = -1.0f;
            if (temperature_sensor_get_celsius(s_sensor, &temperature) == ESP_OK) {
                s_temp = temperature;
            }
        }

        configRUN_TIME_COUNTER_TYPE idle_runtime[CORE_COUNT];
        idle_runtime[0] = ulTaskGetIdleRunTimeCounterForCore(0);
        idle_runtime[1] = ulTaskGetIdleRunTimeCounterForCore(1);

        int64_t now_us = esp_timer_get_time();
        if (!s_have_baseline) {
            s_have_baseline = true;
            s_last_sample_us = now_us;
            s_last_idle[0] = idle_runtime[0];
            s_last_idle[1] = idle_runtime[1];
        } else {
            uint64_t elapsed_us = now_us > s_last_sample_us
                ? (uint64_t)(now_us - s_last_sample_us)
                : 0;

            if (elapsed_us > 0) {
                s_cpu[0] = load_from_idle(idle_runtime[0] - s_last_idle[0], elapsed_us);
                s_cpu[1] = load_from_idle(idle_runtime[1] - s_last_idle[1], elapsed_us);
            }

            s_last_sample_us = now_us;
            s_last_idle[0] = idle_runtime[0];
            s_last_idle[1] = idle_runtime[1];
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void system_metrics_init(void)
{
    nvs_handle_t handle;
    if (nvs_open(NS, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t performance = 0;
        if (nvs_get_u8(handle, KEY_PERF, &performance) == ESP_OK &&
            performance >= 10 && performance <= 100) {
            s_perf = performance;
        }
        nvs_close(handle);
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        metrics_task,
        "metrics",
        4096,
        NULL,
        2,
        &s_task,
        METRICS_CORE
    );

    if (result != pdPASS) {
        s_task = NULL;
    }
}

float system_metrics_temperature(void) { return s_temp; }
uint8_t system_metrics_cpu_load(void) { return s_cpu[0] > s_cpu[1] ? s_cpu[0] : s_cpu[1]; }
uint8_t system_metrics_cpu0_load(void) { return s_cpu[0]; }
uint8_t system_metrics_cpu1_load(void) { return s_cpu[1]; }
uint32_t system_metrics_free_heap(void) { return esp_get_free_heap_size(); }
uint32_t system_metrics_min_heap(void) { return esp_get_minimum_free_heap_size(); }
uint32_t system_metrics_free_psram(void)
{
    return esp_psram_is_initialized() ? heap_caps_get_free_size(MALLOC_CAP_SPIRAM) : 0;
}
uint32_t system_metrics_min_free_psram(void)
{
    return esp_psram_is_initialized() ? heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) : 0;
}
uint32_t system_metrics_uptime_s(void) { return (uint32_t)(esp_timer_get_time() / 1000000ULL); }

esp_err_t system_metrics_set_performance(uint8_t percent)
{
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(handle, KEY_PERF, percent);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) s_perf = percent;
    return err;
}

uint8_t system_metrics_get_performance(void) { return s_perf; }
