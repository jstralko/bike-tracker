#include <Arduino.h>

#include <RadioLib.h>
#include <SPI.h>

// Stage 0: serial only. Stage 1: radio standby. Stage 2: ping/pong gateway.
// Confirm these GPIOs match the FeatherWing's soldered jumpers before upload.
#ifndef FEATHER_TEST_STAGE
#define FEATHER_TEST_STAGE 2
#endif
#ifndef FEATHER_RADIO_CS
#define FEATHER_RADIO_CS 10
#endif
#ifndef FEATHER_RADIO_RST
#define FEATHER_RADIO_RST 11
#endif
#ifndef FEATHER_RADIO_IRQ
#define FEATHER_RADIO_IRQ 6
#endif
#if FEATHER_TEST_STAGE < 0 || FEATHER_TEST_STAGE > 2
#error "FEATHER_TEST_STAGE must be 0 (serial), 1 (radio init), or 2 (ping/pong)"
#endif

#include "hardware.h"
#include "LinkProtocol.h"

namespace {
#if FEATHER_TEST_STAGE >= 1
SX1276 radio = new Module(FEATHER_RADIO_CS, FEATHER_RADIO_IRQ,
                          FEATHER_RADIO_RST, RADIOLIB_NC);
int16_t radioStatus = RADIOLIB_ERR_NONE;
bool radioReady = false;
#if FEATHER_TEST_STAGE == 2
volatile bool packetReceived = false;
// Successful pings are roughly 2.5 s apart; allow a failed exchange plus margin.
constexpr uint32_t kPeerTimeoutMs = linkProtocol::kPeerTimeoutMs;
uint32_t lastValidPing = 0;
uint32_t countedSilenceWindows = 0;
bool seenValidPing = false;

void updatePeerStatus() {
  if (!radioReady) return;  // Keep radio errors visible.
  const uint32_t windows = (millis() - lastValidPing) / kPeerTimeoutMs;
  if (windows > countedSilenceWindows) {
    screen.silenceWindows += windows - countedSilenceWindows;
    countedSilenceWindows = windows;
  }
  const bool silent = windows > 0;
  if (silent && !screen.peerSilent) Serial.println(F("T-Beam silent: no valid ping for 6 seconds"));
  screen.peerSilent = silent;
  screen.status = silent ? "T-BEAM SILENT" : (seenValidPing ? "PING RECEIVED" : "WAITING T-BEAM");
}

void ARDUINO_ISR_ATTR onPacketReceived() { packetReceived = true; }

void listen() {
  packetReceived = false;
  radioStatus = radio.startReceive();
  radioReady = radioStatus == RADIOLIB_ERR_NONE;
  if (radioReady) updatePeerStatus();
  else screen.status = "RX ERROR " + String(radioStatus);
  if (!radioReady) Serial.printf("Start receive failed, code %d\n", radioStatus);
}

void pollRadio() {
  if (!radioReady || !packetReceived) return;
  packetReceived = false;
  String message;
  int16_t state = radio.readData(message);
  if (state == RADIOLIB_ERR_NONE) {
    linkProtocol::logReceived(radio, message);
    if (linkProtocol::isPing(message)) {
      tbeamPosition = linkProtocol::GpsPayload{};
      const int separator = message.indexOf('|');
      tbeamPositionReceived = separator >= 0 && linkProtocol::parseGpsPong(
          message.c_str(), message.substring(0, separator).c_str(), tbeamPosition);
      tbeamPositionUpdated = millis();
      if (screen.peerSilent) Serial.println(F("T-Beam back: valid ping received"));
      lastValidPing = millis();
      countedSilenceWindows = 0;
      seenValidPing = true;
      screen.peerSilent = false;
      screen.received(message, radio.getRSSI(), radio.getSNR());
      pollGps();
      if (tbeamDistanceAvailable()) Serial.printf("T-Beam approximately %.0f m away\n", distanceToTbeam());
      linkProtocol::GpsPayload position;
      const char *status = gpsStatus();
      position.status = !strcmp(status, "FIX") ? 1 : (!strcmp(status, "NO FIX") ? 0 : 2);
      position.satellites = position.status == 2 ? 0 : gps.satellites;
      if (position.status == 1) {
        position.latitude = gps.latitudeDegrees;
        position.longitude = gps.longitudeDegrees;
      }
      String pong = linkProtocol::gpsPongFor(message, position);
      delay(linkProtocol::kReplyDelayMs);  // Give the T-Beam time to switch from TX to RX.
      state = radio.transmit(pong);
      if (state == RADIOLIB_ERR_NONE) {
        ++screen.tx;
        screen.packet = pong;
        Serial.printf("TX  %s | pongs=%lu\n", pong.c_str(),
                      static_cast<unsigned long>(screen.tx));
      } else {
        screen.packet = "TX ERROR " + String(state);
        ++screen.misses;
        Serial.printf("TX failed, code %d\n", state);
      }
    } else {
      Serial.println(F("Ignoring packet: expected PING:<sequence>"));
    }
  } else {
    screen.packet = "RX ERROR " + String(state);
    ++screen.misses;
    Serial.printf("RX read failed, code %d\n", state);
  }
  listen();
}
#endif

void initializeRadio() {
  // Radio and TFT share SPI; each driver uses its own chip select.
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  SPI.begin(SCK, MISO, MOSI);
  Serial.printf("Radio pins: CS=%d RST=%d IRQ=%d | SCK=%d MISO=%d MOSI=%d\n",
                FEATHER_RADIO_CS, FEATHER_RADIO_RST, FEATHER_RADIO_IRQ,
                SCK, MISO, MOSI);
  radioStatus = linkProtocol::initializeRadio(radio);
  if (radioStatus == RADIOLIB_ERR_NONE) radioStatus = radio.standby();
  radioReady = radioStatus == RADIOLIB_ERR_NONE;
  screen.status = radioReady ? "RADIO READY" : "INIT ERROR " + String(radioStatus);
  if (radioReady) {
#if FEATHER_TEST_STAGE == 2
    radio.setPacketReceivedAction(onPacketReceived);
    lastValidPing = millis();
    listen();
    if (radioReady) Serial.println(F("Listening at 915 MHz | test=ping-pong"));
#else
    Serial.println(F("Radio ready at 915 MHz | mode=standby | no transmission"));
#endif
  } else {
    Serial.print(F("Radio initialization failed, RadioLib code "));
    Serial.println(radioStatus);
    if (radioStatus == RADIOLIB_ERR_CHIP_NOT_FOUND) {
      Serial.println(F("Check RFM95W power, SPI connections, and CS/RST jumpers"));
    }
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  beginScreen();
  beginBattery();
  beginGps();
  Serial.println(F("Feather ESP32-S3 TFT bring-up"));
#if FEATHER_TEST_STAGE >= 1
  initializeRadio();
#endif
  Serial.println(F("Setup complete"));
}

void loop() {
  pollGps();
  logGps();
  updateBattery();
#if FEATHER_TEST_STAGE == 2
  pollRadio();
  updatePeerStatus();
#endif
  updateScreen();
#if FEATHER_TEST_STAGE != 2
  static uint32_t previousHeartbeat = 0;
  static uint32_t heartbeat = 0;
  if (millis() - previousHeartbeat >= 1000) {
    previousHeartbeat = millis();
    Serial.printf("Heartbeat %lu | uptime_ms=%lu | stage=%d\n",
                  static_cast<unsigned long>(heartbeat++),
                  static_cast<unsigned long>(millis()), FEATHER_TEST_STAGE);
  }
#endif
  delay(1);
}
