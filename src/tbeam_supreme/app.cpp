#include "app.h"
#include <Arduino.h>
#include <Wire.h>

#include "config.h"

#if TBEAM_TEST_STAGE >= 2
#include <RadioLib.h>
#include <SPI.h>
#endif
#if TBEAM_TEST_STAGE >= 1

#include <XPowersLib.h>
#endif

#include "display.h"
#if TBEAM_TEST_STAGE >= 2
#include "LinkProtocol.h"
#endif

namespace {
using namespace boardConfig;
#if TBEAM_TEST_STAGE >= 1
XPowersPMU pmu;
#endif
#if TBEAM_TEST_STAGE >= 2
SPIClass radioSpi(FSPI);
SX1262 radio = new Module(kRadioCs, kRadioDio1, kRadioReset, kRadioBusy, radioSpi);
#if TBEAM_TEST_STAGE >= 3
volatile bool packetReceived = false;
uint32_t sequence = 0;

void ARDUINO_ISR_ATTR onPacketReceived() {
  packetReceived = true;
}
#endif

[[noreturn]] void stopWithError(const __FlashStringHelper *what, int16_t code) {
  Serial.print(what);
  Serial.print(F(" failed, RadioLib code "));
  Serial.println(code);
  screen.status = "RADIO ERROR";
  screen.packet = "Code " + String(code);
  while (true) screenDelay(1000);
}
#endif

#if TBEAM_TEST_STAGE >= 1
void enableRadioPower() {
  if (!pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS, kPmuSda, kPmuScl)) {
    Serial.println(F("AXP2101 not found; cannot power the LoRa radio."));
    while (true) delay(1000);
  }
  // OLED/sensor rail; leave the ESP32's DC1 rail at its existing setting.
  pmu.setALDO1Voltage(3300);
  pmu.enableALDO1();
  pmu.setALDO3Voltage(3300);
  pmu.enableALDO3();
#if TBEAM_TEST_STAGE >= 3
  pmu.setALDO4Voltage(3300);
  pmu.enableALDO4();
#endif
  delay(100);
}
#endif
}  // namespace

void tbeam::begin() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("T-Beam Supreme bring-up"));
  Serial.print(F("Test stage: "));
  Serial.println(TBEAM_TEST_STAGE);

#if TBEAM_TEST_STAGE >= 1
  Serial.println(F("Initializing AXP2101 and enabling radio power..."));
  enableRadioPower();
  Serial.println(F("AXP2101 ready; ALDO3 enabled"));
#endif
  beginScreen();
#if TBEAM_TEST_STAGE >= 3
  beginLocalGps();
#endif
#if TBEAM_TEST_STAGE >= 2
  Serial.println(F("Initializing SX1262..."));
  radioSpi.begin(kRadioSck, kRadioMiso, kRadioMosi, kRadioCs);

  int16_t state = linkProtocol::initializeRadio(radio);
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Radio init"), state);
#if TBEAM_TEST_STAGE >= 3
  radio.setPacketReceivedAction(onPacketReceived);
#endif
  Serial.println(F("Radio ready at 915 MHz"));
#endif
  screen.status = TBEAM_TEST_STAGE >= 2 ? "RADIO READY" : "SERIAL READY";
  Serial.println(F("Setup complete"));
}

void tbeam::tick() {
  pollLocalGps();
  static uint32_t previousGpsLog = 0;
  if (millis() - previousGpsLog >= 2000) {
    previousGpsLog = millis();
    Serial.printf("T-Beam GPS %s | satellites=%lu", localGpsStatus(), localGps.satellites.value());
    if (proximityAvailable()) {
      const double distance = featherDistance();
      Serial.printf(" | Feather distance=%.1fm | %s", distance,
                    distance <= linkProtocol::kCloseMeters ? "CLOSE" : "FAR");
    }
    Serial.println();
  }
#if TBEAM_TEST_STAGE >= 3
  linkProtocol::GpsPayload ownPosition;
  ownPosition.status = localGpsFresh() ? 1 : (!strcmp(localGpsStatus(), "NO DATA") ? 2 : 0);
  ownPosition.satellites = ownPosition.status == 2 ? 0 : localGps.satellites.value();
  if (ownPosition.status == 1) {
    ownPosition.latitude = localGps.location.lat();
    ownPosition.longitude = localGps.location.lng();
  }
  String ping = linkProtocol::withGps(linkProtocol::ping(sequence++), ownPosition);
  const String expectedReply = linkProtocol::pongFor(ping);
  static uint32_t matched = 0;
  static uint32_t timeouts = 0;
  Serial.print(F("TX  "));
  Serial.println(ping);
  screen.status = screen.peerSilent ? "FEATHER SILENT" : "SENDING PING";
  screen.packet = ping;
  updateScreen();

  int16_t state = radio.transmit(ping);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("TX failed, code "));
    Serial.println(state);
    screen.status = "TX ERROR"; screen.packet = "Code " + String(state);
    screenDelay(linkProtocol::kPingIntervalMs);
    return;
  }

  ++screen.tx;
  screen.status = screen.peerSilent ? "FEATHER SILENT" : "WAITING PONG";
  packetReceived = false;
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Start receive"), state);

  const uint32_t deadline = millis() + linkProtocol::kPongTimeoutMs;
  while (!packetReceived && static_cast<int32_t>(deadline - millis()) > 0) {
    pollLocalGps();
    updateScreen();
    delay(1);
  }

  if (packetReceived) {
    String reply;
    state = radio.readData(reply);
    if (state == RADIOLIB_ERR_NONE) {
      linkProtocol::logReceived(radio, reply);
      linkProtocol::GpsPayload position;
      if (linkProtocol::parseGpsPong(reply.c_str(), expectedReply.c_str(), position)) {
        remoteGps = position;
        remoteGpsReceived = true;
        remoteGpsUpdated = millis();
        Serial.printf("Feather GPS | status=%u satellites=%u", position.status, position.satellites);
        if (position.status == 1) Serial.printf(" | lat=%.6f lon=%.6f", position.latitude, position.longitude);
        Serial.println();
        if (screen.peerSilent) Serial.println(F("Feather back: matching pong received"));
        screen.peerSilent = false;
        screen.received(reply, radio.getRSSI(), radio.getSNR());
        screen.status = "LINK OK";
        Serial.printf("PONG matched | matched=%lu\n",
                      static_cast<unsigned long>(++matched));
      } else {
        screen.status = screen.peerSilent ? "FEATHER SILENT" : "WRONG REPLY";
        Serial.printf("Unexpected reply; expected %s\n", expectedReply.c_str());
      }
    } else {
      screen.status = "RX ERROR"; screen.packet = "Code " + String(state);
      Serial.print(F("RX read failed, code "));
      Serial.println(state);
    }
  } else {
    if (!screen.peerSilent) Serial.println(F("Feather silent: no matching pong before timeout"));
    screen.peerSilent = true;
    screen.status = "FEATHER SILENT"; ++screen.misses;
    Serial.printf("RX  timeout | timeouts=%lu\n",
                  static_cast<unsigned long>(++timeouts));
  }

  state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Standby"), state);

  screenDelay(linkProtocol::kPingIntervalMs);
#else
  // Repeat so opening the USB monitor after boot still shows useful output.
  static uint32_t heartbeat = 0;
  Serial.print(F("Heartbeat "));
  Serial.print(heartbeat++);
  Serial.print(F(" | uptime_ms="));
  Serial.print(millis());
  Serial.print(F(" | stage="));
  Serial.println(TBEAM_TEST_STAGE);
  screenDelay(1000);
#endif
}
