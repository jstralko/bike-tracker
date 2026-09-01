# Bike tracker LoRa ping/pong

Minimal Arduino/PlatformIO firmware proving a two-way 915 MHz LoRa link between:

- LILYGO T-Beam Supreme (ESP32-S3, SX1262, L76K variant), which sends `PING:n`
- Adafruit Feather M0 + RFM95W Radio FeatherWing + Ultimate GPS FeatherWing, which replies `PONG:n`

The Feather also prints its current GPS fix when a ping arrives. GPS is deliberately not included in the radio packet yet, so radio bring-up still works indoors or before a fix is available.

## Important

Attach the correct 915 MHz antennas **before powering or transmitting**. Both boards must use a frequency legal for your location. This example uses 915 MHz, intended for regions where that ISM allocation is permitted.

This project assumes the T-Beam has the **SX1262** radio, not the visually similar LR1121 version.

## Feather wiring

Stack the Ultimate GPS FeatherWing normally. On the RFM95W Radio FeatherWing, solder the three jumper wires recommended for a Feather M0:

| Radio FeatherWing signal | Feather M0 pin |
| --- | --- |
| CS | 10 (`B`) |
| RST | 11 (`A`) |
| IRQ | 6 (`D`) |

MOSI, MISO, and SCK are connected by the FeatherWing headers. Set the Ultimate GPS FeatherWing switch/jumpers for hardware serial (`Serial1`, pins 0/1) according to its silkscreen. The radio IRQ uses pin 6, so it does not conflict with GPS serial.

If the base Feather is not a Feather M0, change the three `kRadio...` constants in `src/feather_gateway/main.cpp` and add/change the matching PlatformIO board environment. FeatherWing IRQ wiring varies by Feather model.

## Build and upload

Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode), then open this repository.

Build and upload the T-Beam:

```sh
pio run -e tbeam_supreme
pio run -e tbeam_supreme -t upload
pio device monitor -b 115200
```

Then disconnect the monitor, connect the Feather, and run:

```sh
pio run -e feather_m0
pio run -e feather_m0 -t upload
pio device monitor -b 115200
```

If more than one serial device is connected, add `--upload-port /dev/your-device` to upload or `--port /dev/your-device` to the monitor command.

Expected T-Beam output:

```text
TX  PING:0
RX  PONG:0  RSSI=-45.00 dBm  SNR=9.50 dB
```

Expected Feather output:

```text
RX  PING:0  RSSI=-43.00 dBm  GPS=no-fix
TX  PONG:0
```

## Shared radio settings

Both sketches explicitly use 915.0 MHz, 125 kHz bandwidth, spreading factor 9, coding rate 4/7, sync word `0x12`, CRC, and an 8-symbol preamble. Change radio settings on **both** ends together.

The T-Beam code also initializes its AXP2101 power manager and enables ALDO3, the rail supplying its LoRa radio.

## Current scope

This is intentionally only a link test: no addressing, retries, authentication, encryption, sleep mode, display, or GPS payload. A practical next step is to put the T-Beam's L76K position and battery state into a compact packet, then add acknowledgements and retry limits.

