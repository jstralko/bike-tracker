#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include "LinkScreen.h"

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
#include <Wire.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#endif

namespace {
Adafruit_SH1106G display(128, 64, &Wire, -1);
LinkScreen screen;
bool displayReady = false;
void updateScreen() {
  static uint32_t previous = 0;
  if (!displayReady || millis() - previous < 150) return;
  previous = millis();
  display.clearDisplay();
  screen.draw(display, "T-BEAM / TRACKER", SH110X_WHITE);
  display.display();
}
void screenDelay(uint32_t duration) {
  uint32_t started = millis();
  while (millis() - started < duration) { updateScreen(); delay(1); }
}
void beginScreen() {
  Wire.begin(17, 18);
  Wire.setClock(400000);
  // Prefer 0x3D on newer boards; 0x3C may be the magnetometer.
  for (uint8_t address : {uint8_t(0x3D), uint8_t(0x3C)}) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      displayReady = display.begin(address, true);
      if (displayReady) { Serial.printf("OLED ready at 0x%02X\n", address); break; }
    }
  }
  if (!displayReady) Serial.println(F("OLED unavailable; serial logging continues"));
  updateScreen();
}
#if TBEAM_TEST_STAGE >= 2
constexpr float kFrequencyMHz = 915.0;
constexpr float kBandwidthKHz = 125.0;
constexpr uint8_t kSpreadingFactor = 9;
constexpr uint8_t kCodingRate = 7;
constexpr uint8_t kSyncWord = 0x12;
constexpr int8_t kTxPowerDbm = 17;
constexpr uint16_t kPreambleLength = 8;

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

  int16_t state = radio.begin(kFrequencyMHz, kBandwidthKHz,
                              kSpreadingFactor, kCodingRate, kSyncWord,
                              kTxPowerDbm, kPreambleLength);
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Radio init"), state);
  state = radio.setCRC(true);
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Enable CRC"), state);
#if TBEAM_TEST_STAGE >= 3
  radio.setPacketReceivedAction(onPacketReceived);
#endif
  Serial.println(F("Radio ready at 915 MHz"));
#endif
  screen.status = "RADIO READY";
  Serial.println(F("Setup complete"));
}

void loop() {
#if TBEAM_TEST_STAGE >= 3
  String ping = "PING:" + String(sequence++);
  const String expectedReply = "PONG:" + ping.substring(5);
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
    screenDelay(2000);
    return;
  }

  ++screen.tx;
  screen.status = screen.peerSilent ? "FEATHER SILENT" : "WAITING PONG";
  packetReceived = false;
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Start receive"), state);

  const uint32_t deadline = millis() + 1500;
  while (!packetReceived && static_cast<int32_t>(deadline - millis()) > 0) {
    updateScreen();
    delay(1);
  }

  if (packetReceived) {
    String reply;
    state = radio.readData(reply);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.print(F("RX  "));
      Serial.print(reply);
      Serial.print(F("  RSSI="));
      Serial.print(radio.getRSSI());
      Serial.print(F(" dBm  SNR="));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));
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

  screenDelay(2000);
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
