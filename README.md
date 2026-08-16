# ESP32_S3_Nat_Extander/ESP32

A high-performance ESP32-S3 Wi-Fi NAT extender designed for local network sharing, fast Wi-Fi recovery, persistent configuration and long-running operation.

## What it does

The ESP32-S3 works in **AP + STA** mode:

1. **STA** connects to the upstream Wi-Fi.
2. **AP** creates the extender's local Wi-Fi network.
3. Connected clients send traffic through the ESP32-S3.
4. **NAT/NAPT** forwards that traffic to the upstream network.
5. If the upstream Wi-Fi is lost, the router performs a controlled channel-recovery scan and reconnects automatically.

The normal recovery profile prioritizes **channels 1, 6 and 11**. A **full-channel recovery mode** can be selected when wider channel searching is required. Recovery uses bounded backoff so repeated failures do not create an uncontrolled scan loop.

## Main Features

- ESP32-S3 AP + STA networking
- NAT/NAPT routing
- Fast Wi-Fi reconnect and channel recovery
- 1/6/11 normal recovery mode
- Optional full-channel recovery
- Persistent settings with NVS
- Authenticated local dashboard
- Desktop-style dashboard after login
- Live Wi-Fi, traffic and system information
- Signal LEDs: **Red / Yellow / Green**
- Automatic 5 V cooling-fan control
- CPU temperature monitoring
- LittleFS logging
- Dual OTA application partitions
- GitHub Actions build and artifact generation

## Configuration

The project is primarily configured from its firmware configuration/source rather than requiring an external cloud service.

Persistent settings are stored in **NVS**, so values intended to survive reboot remain available after restart.

The dashboard is used for supported runtime configuration and monitoring. Depending on the selected recovery mode, Wi-Fi recovery can use:

- **Normal:** channels 1, 6, 11
- **Full:** complete supported 2.4 GHz channel scan

The selected mode remains active until changed.

### LEDs

Three status LEDs provide a quick signal indication:

- 🔴 Red — poor signal
- 🟡 Yellow — medium signal
- 🟢 Green — good signal

### Cooling fan

The firmware monitors the ESP32-S3 temperature and controls the external **5 V fan** automatically. Temperature hysteresis prevents rapid ON/OFF cycling around the threshold.

**Hardware note:** LED and fan GPIOs, polarity and the fan driver circuit must match the actual ESP32-S3 board. Do not connect a 5 V fan directly to an ESP32 GPIO.

## Hardware Connection

Connect the ESP32-S3 to the computer through its USB interface (or a suitable USB-to-UART interface on boards without native USB).

For first testing:

1. Connect USB/power.
2. Confirm the board appears as a serial/USB device.
3. Flash the firmware.
4. Open a serial monitor if diagnostic output is required.
5. Configure the router through its local Wi-Fi/dashboard.
6. Test NAT traffic before connecting the external LEDs and fan permanently.

The exact LED/fan GPIO wiring must follow the GPIO definitions in the source used for the build.

## Build

This project uses **ESP-IDF** and targets **ESP32-S3**.

```bash
idf.py set-target esp32s3
idf.py build
idf.py size
idf.py merge-bin -o build/merged.bin -f raw
```

The successful CI build generated:

- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`
- `build/ota_data_initial.bin`
- `build/esp32s3_nat_router.bin`
- `build/merged.bin`
- `build/flash_args`

The current application image is about **0.87 MiB**, leaving about **57% free** in the 2 MiB application partition.

## Flash Layout

The generated partition table is:

| Address | Size | Purpose |
|---:|---:|---|
| `0x000000` | — | Bootloader image |
| `0x008000` | — | Partition table |
| `0x009000` | 24 KiB | NVS |
| `0x00F000` | 8 KiB | OTA data |
| `0x011000` | 4 KiB | PHY initialization area |
| `0x020000` | 2 MiB | `ota_0` application |
| `0x220000` | 2 MiB | `ota_1` application |
| `0x420000` | 12160 KiB | LittleFS storage |

## Flashing

The CI build is configured for:

- **Chip:** ESP32-S3
- **Flash size:** 16 MB
- **Flash mode:** DIO
- **Flash frequency:** 80 MHz
- **Baud rate:** 460800

### Complete merged image

The simplest method is the generated `merged.bin` at address `0x000000`:

```bash
python -m esptool --chip esp32s3 \
  -b 460800 \
  --before default-reset \
  --after hard-reset \
  write-flash \
  --flash-mode dio \
  --flash-size 16MB \
  --flash-freq 80m \
  0x0 build/merged.bin
```

### Individual firmware files

The build system also provides the exact addresses through `build/flash_args`. The current application/bootloader layout uses:

```text
0x000000  bootloader/bootloader.bin
0x008000  partition_table/partition-table.bin
0x00F000  ota_data_initial.bin
0x020000  esp32s3_nat_router.bin
```

Using the generated `flash_args` is preferred when flashing individual images because it reflects the build's actual configuration.

## Important

- Do **not** mix binaries from different builds.
- The merged image belongs at **0x000000**.
- Use a stable 5 V power source capable of supplying the ESP32-S3 and external fan hardware.
- A 5 V fan requires an appropriate transistor/MOSFET driver and flyback protection where applicable.
- Actual 24/7 reliability must be validated on the target board under sustained traffic and thermal load.

## Current Build Result

The supplied CI log shows a **successful complete build and merge**:

- Firmware image generated successfully
- Partition-size check passed
- Size analysis completed
- `merged.bin` generated successfully
- All required artifacts verified
- GitHub Actions artifact upload completed successfully

The Kconfig messages concerning a few unused/default boolean values are **warnings/notes from ESP-IDF configuration parsing, not build failures**. The build completed successfully despite them.

The GitHub Actions runner's Node.js deprecation notices are also CI environment notices and are not firmware build errors.
