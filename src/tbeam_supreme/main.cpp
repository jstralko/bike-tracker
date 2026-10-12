#include <Arduino.h>
#include <Wire.h>

// Start at 0 and increase one stage at a time; see TODO.md.
// 0 = serial only, 1 = PMU power, 2 = radio init, 3 = ping/pong.
// The preprocessor comments out hardware code for the inactive stages.
#define TBEAM_TEST_STAGE 3

#if TBEAM_TEST_STAGE < 0 || TBEAM_TEST_STAGE > 3
#error "TBEAM_TEST_STAGE must be between 0 and 3"
#endif

#if TBEAM_TEST_STAGE >= 2
#include <RadioLib.h>
#include <SPI.h>
#endif
#if TBEAM_TEST_STAGE >= 1

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#endif

#include "display.h"
#if TBEAM_TEST_STAGE >= 2
#include "LinkProtocol.h"
#endif

namespace {
#if TBEAM_TEST_STAGE >= 2
constexpr int kRadioSck = 12;
constexpr int kRadioMiso = 13;
constexpr int kRadioMosi = 11;
constexpr int kRadioCs = 10;
constexpr int kRadioReset = 5;
constexpr int kRadioDio1 = 1;
constexpr int kRadioBusy = 4;
#endif
#if TBEAM_TEST_STAGE >= 1
constexpr int kPmuSda = 42;
constexpr int kPmuScl = 41;

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
  delay(100);
}
#endif
}  // namespace

void setup() {
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

void loop() {
#if TBEAM_TEST_STAGE >= 3
  String ping = linkProtocol::ping(sequence++);
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
    updateScreen();
    delay(1);
  }

  if (packetReceived) {
    String reply;
    state = radio.readData(reply);
    if (state == RADIOLIB_ERR_NONE) {
      linkProtocol::logReceived(radio, reply);
      if (reply == expectedReply) {
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
