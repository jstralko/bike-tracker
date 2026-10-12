# Bike tracker LoRa ping/pong

Minimal Arduino/PlatformIO firmware proving a two-way 915 MHz LoRa link between:

- LILYGO T-Beam Supreme (ESP32-S3, SX1262, L76K variant), which sends `PING:n`
- Adafruit Feather M0 + RFM95W Radio FeatherWing + Ultimate GPS FeatherWing, which replies `PONG:n`

The Feather also prints its current GPS fix when a ping arrives. GPS is deliberately not included in the radio packet yet, so radio bring-up still works indoors or before a fix is available.

The T-Beam now defaults to **stage 3, ping/pong**. Earlier serial, PMU, and radio initialization stages remain available; see [the bring-up checklist](TODO.md).

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

### Feather ESP32-S3 TFT ping/pong gateway

Use **`feather_esp32s3_tft`** for the Feather and **`tbeam_supreme`** for
the T-Beam. The default stages now enable ping/pong on both boards. The
Feather's verified wiring is CS→B/GPIO10, IRQ→D/GPIO6, and radio RST→GPIO11
through the repaired bypass. GPS is not enabled. Both screens show a live link dashboard.

Attach both 915 MHz antennas, close the target board's monitor, and upload:

```sh
pio run -e feather_esp32s3_tft -t upload
pio run -e tbeam_supreme -t upload
```

Open each monitor in a separate terminal:

```sh
pio device monitor -e feather_esp32s3_tft
pio device monitor -e tbeam_supreme
```

Expected Feather output:

```text
Listening at 915 MHz | test=ping-pong
RX  PING:0  RSSI=-45.00 dBm  SNR=9.50 dB
TX  PONG:0 | pongs=1
```

Expected T-Beam output:

```text
TX  PING:0
RX  PONG:0  RSSI=-45.00 dBm  SNR=9.50 dB
PONG matched | matched=1
```

Signal measurements vary. With the gateway off, the T-Beam reports
`RX  timeout` and keeps trying. The Feather also prints a heartbeat every
second. If initialization or reception setup fails, its heartbeat reports
`radio=failed` and the RadioLib code. Invalid packets do not receive a pong;
RX/TX failures are logged and the Feather attempts to resume listening.

To repeat earlier tests, set `FEATHER_TEST_STAGE=0` (serial) or `1` (radio
standby) in `platformio.ini`, and `TBEAM_TEST_STAGE=0`, `1`, or `2` in the
T-Beam source. See [the checklist](TODO.md).

### T-Beam and original Feather M0 gateway

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

Expected T-Beam output at the default stage `0`:

```text
T-Beam Supreme bring-up
Test stage: 0
Setup complete
Heartbeat 0 | uptime_ms=1500 | stage=0
Heartbeat 1 | uptime_ms=2500 | stage=0
```

Uptime values vary slightly, and startup lines may be missed when the monitor opens after boot.

Expected T-Beam output at stage `3`:

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

At stages `1` and above, the T-Beam code initializes its AXP2101 power manager and enables ALDO3, the rail supplying its LoRa radio.

## Current scope

This is intentionally only a link test: no addressing, retries, authentication, encryption, sleep mode, or GPS payload. A practical next step is to put the T-Beam's L76K position and battery state into a compact packet, then add acknowledgements and retry limits.

## Live screen dashboards

The Feather TFT shows a cyan dashboard with link state, TX/RX counts, the last
packet, RSSI/SNR, error count, and packet age. The T-Beam SH1106 OLED shows a
compact version with LINK OK, WAITING PONG, TIMEOUT, or radio errors. Both have
an animated moving dot and bars indicating the last received signal strength;
the dot is a running indicator, not proof of a successful packet.

The Feather buffers its frame to reduce flicker and shares SPI with the radio
using separate chip selects. Screen refreshes are limited to roughly 8 Hz.
The T-Beam uses I2C GPIO17/18, enables the OLED ALDO1 power rail, and probes
0x3D before 0x3C to accommodate board revisions. If no display is detected,
serial logging and radio operations continue. Actual screen orientation and
radio operation with refreshes enabled still need checking on the boards.
