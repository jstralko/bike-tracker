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
through the repaired bypass. The Feather initializes its GPS locally; ping/pong packets still contain only sequence numbers. Both screens show a live link dashboard.

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

The Feather peer indicator is based only on valid `PING:<sequence>` packets.
It shows WAITING T-BEAM initially, PING RECEIVED after traffic, and an orange
T-BEAM SILENT screen after six seconds without a valid ping. Silence pauses
animation and clears signal bars. GAPS counts cumulative six-second silence
intervals (including waiting at startup); ERR counts radio read/transmit errors.
LAST is the age of the last valid ping, and RSSI/SNR remain its historical
measurements. A new valid ping restores the live state automatically; unrelated
packets do not keep the peer indicator alive. A transmitted pong does not prove
it reached the T-Beam; check the T-Beam's PONG matched log for that direction.

The T-Beam shows FEATHER SILENT after a pong receive timeout (1.5 seconds).
Its monochrome warning alternates normal/inverted every half-second, clears
signal bars, and pauses the moving dot. The warning stays visible during later
ping attempts until a matching pong restores LINK OK. Unexpected packets do
not restore the peer indicator or replace the last matching pong's signal data.
The existing timeout count continues in serial logs. Radio errors are still
shown as errors rather than being hidden by the silent-peer warning.

The Feather dashboard includes battery percentage, voltage, and a fill icon,
updated every five seconds. Below 20% it turns red. It auto-detects the onboard
MAX17048 or older LC709203F gauge; missing/invalid readings show BAT -- rather
than a fabricated percentage. A connected single-cell LiPo is needed for a
meaningful reading; USB-only operation cannot reliably identify battery presence
from gauge readings. Percentage is an estimate, not a charging indicator.
The LC709203F currently uses Adafruit's 500 mAh profile; adjust setPackSize in
src/feather_gateway/hardware.h for your battery capacity if this older gauge is detected.

## Firmware organization

Each board's `main.cpp` owns setup and its ping/pong flow. One shared helper
layer in `include/LinkProtocol.h` defines radio settings, packet construction,
validation, timing constants, and received-packet logging; there is no driver
wrapper or inheritance hierarchy. `include/LinkScreen.h` renders the shared
view. Feather display/battery helpers are in `src/feather_gateway/hardware.h`;
T-Beam display helpers are in `src/tbeam_supreme/display.h`. Wiring and PMU
setup remain explicit on their respective boards. The unused M0 gateway is
preserved under `examples/legacy_feather_m0` and is not part of either build.

## Feather GPS bring-up

Side-by-side Ultimate GPS FeatherWing wiring: 3V→Feather 3V, GND→GND,
GPS TX breakout→Feather RX/GPIO2, GPS RX breakout→Feather TX/GPIO1.
GPS helpers in `src/feather_gateway/gps.h` start Serial1 at 9600 baud, request
RMC/GGA sentences at 1 Hz, and poll without waiting for a satellite fix.
The UART has a 1024-byte receive buffer to accommodate radio/display activity.

The TFT shows GPS NO DATA (no recent UART bytes), NO NMEA (bytes arriving but
no recent valid navigation sentences), NO FIX (working GPS, no satellite fix),
or FIX plus satellite count. Navigation data older than five seconds is stale;
old coordinates are not logged as a live fix. Serial logs every two seconds
include status, satellite count, parse errors, and coordinates/altitude when
freshly fixed. Satellite count is the last parsed value. No raw NMEA flood.

Upload `feather_esp32s3_tft` and test outdoors with the ceramic antenna facing
upward. A UART-start message only confirms software initialization: valid NMEA
confirms communication with the GPS. GPS reception and simultaneous ping/pong
still need verification on hardware. No GPS data is added to PING/PONG yet.

The Feather dashboard shows signed decimal LAT/LON to six decimal places below
GPS status. Without a fresh fix, both show --. The compact radio rows preserve
link status, counts, signal readings, last packet, gaps/errors, battery, and
animation while making space for coordinates. Display precision is not GPS
accuracy. The T-Beam screen layout is unchanged.

## GPS in pong replies

Upload **both boards** for the GPS payload protocol. T-Beam still sends PING:n;
Feather returns PONG:n|GPS:1,status,satellites,latitude,longitude. Example:
`PONG:42|GPS:1,1,5,37.123456,-122.123456`. The first 1 is protocol version;
status is 0=no fix, 1=fresh fix, 2=unavailable/stale. Without a fresh fix,
coordinates are zero placeholders and are not displayed as a position.
Only the Feather GPS is sent; the T-Beam's own GPS is not initialized yet.

T-Beam validates the exact sequence, version, fields, finite coordinates and
geographic bounds before accepting the pong. It logs the remote GPS data and
alternates its OLED between radio details and Feather coordinates every four
seconds. No-fix or stale data displays GPS NO FIX/UNAVAILABLE. Coordinate display
expires after six seconds without an accepted pong or when the link is silent.
Feather's own screen retains its local coordinates and battery status.

Native parser tests: `g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/gps_payload_test.cpp -o /tmp/bike-gps-payload-test` then run the executable.

## GPS proximity on the T-Beam

T-Beam now powers its L76K via ALDO4, keeps GPIO7 high, and listens at 9600
baud on RX=9/TX=8, following LilyGO's GPS example pin definitions. TinyGPSPlus
parses continuously during reply waits and between pings. It does not wait for
GPS fix at startup. The existing Feather GPS pong format stays unchanged.

The OLED cycles radio, Feather coordinates, and proximity pages every four
seconds. With fresh positions on both boards, haversine surface distance is
shown in meters: CLOSE is <=20 m, FAR is >20 m. Without a fresh local fix,
remote fix, or recent accepted pong, distance is --. Link silence overrides
proximity availability. Local location age limit is five seconds; remote pong
age limit is six seconds. GPS error may make CLOSE/FAR fluctuate near 20 m;
this is approximate positioning, not precise ranging.

Upload T-Beam and test both boards outdoors. Logs report T-Beam GPS status,
satellite count, distance, and CLOSE/FAR. Threshold is kCloseMeters in
include/GeoDistance.h. Native distance tests are in tests/geo_distance_test.cpp;
this environment has no native C++ compiler, so those tests are not run here.

## Finding the T-Beam from the Feather

Both directions now include GPS status: T-Beam pings carry its position and
Feather pongs carry the Feather's position. Upload both environments together.
The Feather screen replaces local LAT/LON rows with a large approximate
`~ 123 m away` (or km) estimate to the T-Beam, green within 20 m. Coordinates
remain in logs. Fresh GPS fixes on both boards and a recent valid T-Beam ping
are required. Missing data shows DISTANCE -- with a reason; radio silence
hides the estimate. No direction or heading is inferred from distance.
This measures straight-line surface distance, not walking-route distance.
GPS error may cause small changes even when standing still.

Wire formats share the GPS version-1 fields:
`PING:n|GPS:1,status,satellites,latitude,longitude` and
`PONG:n|GPS:1,status,satellites,latitude,longitude`.
Only the sequence portion is echoed into the pong header. A legacy plain ping
can still get a pong, but cannot provide a distance on the Feather.

## Feather phone-style finder screen

The Feather's screen now focuses on finding the T-Beam: a large approximate
distance and a north-up compass arrow with cardinal bearing. The top-right
status bar contains local GPS, radio-link bars, and a battery icon/percentage.
GPS is green with a fresh local fix; orange otherwise. Radio bars clear when
traffic is stale/silent and turn red on a radio error. Battery is red below 20%.
RSSI/SNR, packet counts, voltage, and GPS coordinates remain available in serial
logs rather than occupying the finder screen.

The arrow is a geographic bearing (0=N, 90=E), not relative to how the device
is held. There is no magnetometer/heading sensor on this Feather setup. Align
the screen's north with real north to use the arrow. At separations under 10 m,
direction is suppressed in favor of NEARBY to avoid unstable GPS bearings.
Missing/stale fixes show --, with the reason in the footer. Within 20 m the
valid distance turns green. Hardware layout/bearing verification remains pending.

The Feather shows a five-second BIKE FINDER startup animation with a radar
sweep, expanding pulse, blinking target, and progress bar before opening the
finder screen. Animation begins on the first rendered frame after hardware
setup; GPS parsing, battery polling, and radio replies continue in the loop.
The progress bar represents splash time, not GPS-fix or hardware readiness.
