#include <Arduino.h>

// The ESP32-S3 TFT environment enables this main-board-only test.
// Keep the original M0 gateway below disabled until wing wiring is verified.
#ifndef FEATHER_MAIN_BOARD_TEST
#define FEATHER_MAIN_BOARD_TEST 0
#endif

#if FEATHER_MAIN_BOARD_TEST
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("Feather ESP32-S3 TFT bring-up: serial only"));
  Serial.println(F("Setup complete"));
}

void loop() {
  static uint32_t heartbeat = 0;
  Serial.print(F("Heartbeat "));
  Serial.print(heartbeat++);
  Serial.print(F(" | uptime_ms="));
  Serial.print(millis());
  Serial.println(F(" | test=serial-only"));
  delay(1000);
}
#else
// Original Feather M0 gateway (excluded from the ESP32-S3 TFT test build).
#include <Adafruit_GPS.h>
#include <RadioLib.h>
#include <SPI.h>

namespace {
constexpr float kFrequencyMHz = 915.0;
constexpr float kBandwidthKHz = 125.0;
constexpr uint8_t kSpreadingFactor = 9;
constexpr uint8_t kCodingRate = 7;
constexpr uint8_t kSyncWord = 0x12;
constexpr int8_t kTxPowerDbm = 17;
constexpr uint16_t kPreambleLength = 8;

// Radio FeatherWing jumpers: CS->10, RST->11, IRQ->6 on Feather M0.
constexpr int kRadioCs = 10;
constexpr int kRadioReset = 11;
constexpr int kRadioIrq = 6;

SX1276 radio = new Module(kRadioCs, kRadioIrq, kRadioReset, RADIOLIB_NC);
Adafruit_GPS gps(&Serial1);
volatile bool packetReceived = false;

void onPacketReceived() {
  packetReceived = true;
}

[[noreturn]] void stopWithError(const __FlashStringHelper *what, int16_t code) {
  Serial.print(what);
  Serial.print(F(" failed, RadioLib code "));
  Serial.println(code);
  while (true) delay(1000);
}

void listen() {
  packetReceived = false;
  int16_t state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Start receive"), state);
}

void pollGps() {
  while (Serial1.available()) gps.read();
  if (gps.newNMEAreceived()) gps.parse(gps.lastNMEA());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("Feather LoRa pong gateway"));

  gps.begin(9600);
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  int16_t state = radio.begin(kFrequencyMHz, kBandwidthKHz,
                              kSpreadingFactor, kCodingRate, kSyncWord,
                              kTxPowerDbm, kPreambleLength);
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Radio init"), state);
  radio.setPacketReceivedAction(onPacketReceived);
  listen();
  Serial.println(F("Listening at 915 MHz"));
}

void loop() {
  pollGps();
  if (!packetReceived) return;

  String message;
  int16_t state = radio.readData(message);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("RX read failed, code "));
    Serial.println(state);
    listen();
    return;
  }

  Serial.print(F("RX  "));
  Serial.print(message);
  Serial.print(F("  RSSI="));
  Serial.print(radio.getRSSI());
  Serial.print(F(" dBm  GPS="));
  if (gps.fix) {
    Serial.print(gps.latitudeDegrees, 6);
    Serial.print(',');
    Serial.println(gps.longitudeDegrees, 6);
  } else {
    Serial.println(F("no-fix"));
  }

  if (message.startsWith("PING:")) {
    String pong = "PONG:" + message.substring(5);
    delay(50);
    state = radio.transmit(pong);
    Serial.print(F("TX  "));
    Serial.println(state == RADIOLIB_ERR_NONE ? pong : "failed");
  }
  listen();
}
#endif  // FEATHER_MAIN_BOARD_TEST
