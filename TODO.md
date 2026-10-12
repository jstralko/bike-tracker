# T-Beam bring-up checklist

For the Feather ESP32-S3 TFT main board, use the separate checklist below. The `feather_m0` environment is the original gateway and is not the correct upload target for your ESP32-S3.

Change `TBEAM_TEST_STAGE` near the top of `src/tbeam_supreme/main.cpp`, then rebuild and upload after each change. Inactive hardware code is excluded by `#if` blocks, preserving it for later tests. The current default is stage **3** after successful power and radio bring-up.

## 0 — Serial logging only

- [X] Connect the T-Beam with a USB data cable.
- [X] Build: `pio run -e tbeam_supreme`.
- [X] Upload: `pio run -e tbeam_supreme -t upload`.
- [X] Open the monitor: `pio device monitor -e tbeam_supreme` (115200 baud).
- [X] Confirm `Heartbeat 0 | uptime_ms=... | stage=0` followed by increasing heartbeat and uptime values roughly once per second. The first number may be higher if the monitor opened after boot.
- [X] Press reset and confirm the counter starts over. USB may reconnect; reopen the monitor if necessary.
- [X] Leave it running for a minute and confirm logs continue without unexpected resets.

If no logs appear, check the selected serial port (`pio device list`), the data cable, and whether USB reconnects after upload/reset. Add `--upload-port /dev/your-device` to upload and `--port /dev/your-device` to the monitor when needed. Startup messages can be missed; repeated heartbeats are the primary check.

## 1 — Power manager

- [X] Set the stage to `1`, rebuild, and upload.
- [X] Confirm `AXP2101 ready; ALDO3 enabled`, `Setup complete`, and continuing heartbeats.
- [X] If `AXP2101 not found` appears, check the board variant and PMU pins before proceeding. This error deliberately stops setup.

## 2 — Radio initialization

- [X] Attach the appropriate antenna before proceeding; verify this board has the SX1262 variant.
- [X] Set the stage to `2`, rebuild, and upload.
- [X] Confirm `Radio ready at 915 MHz`, `Setup complete`, and continuing heartbeats. This stage does not transmit packets.
- [X] If initialization fails, record the RadioLib error code and check the radio variant, power, and pin assignments.

## 3 — Ping/pong link

- [ ] Confirm the configured frequency is permitted in your location and antennas are attached to both boards.
- [ ] Build/upload the Feather gateway using `pio run -e feather_esp32s3_tft -t upload`.
- [ ] Set the T-Beam stage to `3`, rebuild, and upload.
- [ ] Confirm increasing `TX  PING:n` messages. With no gateway, `RX  timeout` is expected.
- [ ] With the gateway running, confirm matching `RX  PONG:n` messages and RSSI/SNR values.
- [ ] Confirm the Feather logs incoming pings and outgoing pongs.

## Later iterations (not implemented yet)

- [ ] Add T-Beam GPS serial input and log parsed data without changing radio packets.
- [ ] Confirm GPS fix outdoors, then add position to packets.
- [ ] Add battery readings and verify them against a known power state.
- [ ] Add packet addressing, acknowledgement checks, and bounded retries.
- [ ] Test battery operation, then introduce sleep/wake behavior.

## Feather ESP32-S3 TFT — ping/pong (default stage 2)

- [x] Verify radio initialization reports `radio=ready | code=0` after header
      repairs and the RST→GPIO11 bypass.
- [ ] Attach antennas to both boards and upload both current environments.
- [ ] Confirm Feather logs `Listening at 915 MHz | test=ping-pong`.
- [ ] Confirm Feather logs `RX  PING:n` and `TX  PONG:n`.
- [ ] Confirm T-Beam logs the matching reply and increasing `PONG matched` count.
- [ ] Power off the Feather and confirm T-Beam timeouts; power it on and confirm
      successful replies resume without resetting the T-Beam.
- [ ] Leave both running and confirm replies keep matching during the field test.

This test includes live screen dashboards; GPS parsing remains disabled.
The Feather stage is selected in `platformio.ini`; T-Beam defaults to stage 3.

## Feather ESP32-S3 TFT — radio initialization

Use `FEATHER_TEST_STAGE=1` to repeat initialization: initialize an
RFM95W/SX1276 at 915 MHz, enable CRC, and enter standby without transmitting.
Set `FEATHER_TEST_STAGE=0` in `platformio.ini` to repeat the serial-only test.

- [ ] Verify the radio is an RFM95W and confirm its soldered CS/RST/IRQ jumpers.
      The proposed configuration is CS=GPIO10, RST=GPIO11, IRQ=GPIO6; change
      `FEATHER_RADIO_*` build flags to match your actual wiring before upload.
- [ ] Connect the FeatherWing SPI headers (SCK=36, MISO=37, MOSI=35), 3.3V,
      and GND, and attach the 915 MHz antenna.
- [ ] Build/upload `feather_esp32s3_tft`, then open its monitor.
- [ ] Confirm `Radio ready at 915 MHz | mode=standby | no transmission` and
      repeating `test=radio-init | radio=ready | code=0` heartbeats.
- [ ] If initialization fails, record the code and check power and wiring.
      Heartbeats continue reporting failure; no automatic retries are performed.

This verifies SPI communication and radio configuration, not RF transmission,
reception, or the IRQ wire. The later ping/pong test verifies those paths.
The TFT chip select is held high because it shares the SPI bus.

## Feather ESP32-S3 TFT — main board only (stage 0)

The `feather_esp32s3_tft` environment sets `FEATHER_MAIN_BOARD_TEST=1`, excluding the original M0 gateway code. Set `FEATHER_TEST_STAGE=0` for this serial-only test. No FeatherWings are needed; the built-in TFT is not initialized yet.

- [ ] Connect only the main Feather board with a USB data cable.
- [ ] In VS Code, expand **PlatformIO → PROJECT TASKS → feather_esp32s3_tft → General**.
- [ ] Select **Upload and Monitor** (or **Upload**, then **Monitor**).
- [ ] Confirm repeating `Heartbeat 0 | uptime_ms=... | test=serial-only` messages at 115200 baud. The first counter may be higher if you opened the monitor later.
- [ ] Press reset, reopen the monitor if USB reconnects, and confirm the counter restarts.
- [ ] Let logs run for a minute and check for unexpected resets.

Equivalent terminal commands:

```sh
pio run -e feather_esp32s3_tft -t upload
pio device monitor -e feather_esp32s3_tft
```

Future main-board tests (not implemented yet):

- [ ] Add a simple built-in TFT color/text test.
- [ ] Add button input logging using the board's pin definitions.
- [ ] Add a built-in LED/NeoPixel test.
- [ ] Test battery operation and battery monitoring.
- [ ] Only after main-board tests pass, verify ESP32-S3 FeatherWing wiring before adding radio/GPS. Do not enable the M0 gateway code as-is on this board; its pin assignments need review.

## Both boards connected in VS Code

The Feather environment disables the board definition's 1200-baud touch/reset and new-port wait, and routes Arduino `Serial` to USB Serial/JTAG (`ARDUINO_USB_MODE=1`). This matches its configured Espressif by-id path and avoids waiting for a UF2 bootloader port during upload. After a manual BOOT+RESET recovery upload, tap RESET without holding BOOT if the sketch does not start.

- [ ] Confirm both boards create USB serial ports: `pio device list` and `ls -l /dev/serial/by-id/` in the VS Code terminal.
- [ ] Identify each board's path by disconnecting/reconnecting one board once. Names alone may not distinguish two ESP32-S3 boards.
- [x] Assign each board's stable path to `upload_port` and `monitor_port` in `platformio.ini`: T-Beam serial `64:E8:33:5E:30:24`, Feather serial `D0:CF:13:0A:E4:B8`. Recheck the Feather's identity after its first firmware upload; bootloader and application identities can differ. Avoid `/dev/ttyACM0`/`1` for permanent configuration because numbering can change.
- [ ] Use each environment's **Upload** task to target that board; **Build** needs no connected board.
- [ ] Open each environment's **Monitor** task in its own terminal to view both logs. Close the target board's monitor before uploading to it if the port is busy; the other monitor can stay open.
- [ ] Upload to each board with both connected, then confirm that board's identifying startup message/heartbeat.

If the Feather has no serial port, hold BOOT, tap RESET, then release BOOT to enter ROM download mode and check the device list again. A device appearing only as a `FTHRS3BOOT` drive is in the UF2 bootloader. USB enumeration in `dmesg` does not by itself guarantee a serial interface exists. Check the USB data cable and kernel messages if no USB device appears at all.

The ROM/UF2 bootloader and running sketch may expose different USB identities. For a first or recovery upload, select the current bootloader port explicitly with `--upload-port /dev/ttyACM...`, then identify the running sketch's path after reset. Do not permanently assign a bootloader-only by-id path as the monitor port.

If a port exists but access is denied, inspect `ls -l /dev/ttyACM*` and `id` in the VS Code terminal. Configure host serial access using the appropriate device group or PlatformIO udev rules, then restart your login session/VS Code after changing group membership.

## Screen dashboard verification

- [ ] Upload both builds and confirm Feather TFT and T-Beam OLED are readable.
- [ ] Confirm counters, packets, and RSSI/SNR update with matching ping/pong.
- [ ] Confirm animation moves while waiting and OLED shows TIMEOUT with gateway off.
- [ ] Check the Feather display orientation; adjust setRotation(3) if needed.
- [ ] Confirm serial packet logs still work and reception remains reliable.
