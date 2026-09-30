#include <Arduino.h>

// Start at 0 and increase one stage at a time; see TODO.md.
// 0 = serial only, 1 = PMU power, 2 = radio init, 3 = ping/pong.
// The preprocessor comments out hardware code for the inactive stages.
#define TBEAM_TEST_STAGE 2

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

void onPacketReceived() {
  packetReceived = true;
}
#endif

[[noreturn]] void stopWithError(const __FlashStringHelper *what, int16_t code) {
  Serial.print(what);
  Serial.print(F(" failed, RadioLib code "));
  Serial.println(code);
  while (true) delay(1000);
}
#endif

#if TBEAM_TEST_STAGE >= 1
void enableRadioPower() {
  if (!pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS, kPmuSda, kPmuScl)) {
    Serial.println(F("AXP2101 not found; cannot power the LoRa radio."));
    while (true) delay(1000);
  }
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
#if TBEAM_TEST_STAGE >= 2
  Serial.println(F("Initializing SX1262..."));
  radioSpi.begin(kRadioSck, kRadioMiso, kRadioMosi, kRadioCs);

  int16_t state = radio.begin(kFrequencyMHz, kBandwidthKHz,
                              kSpreadingFactor, kCodingRate, kSyncWord,
                              kTxPowerDbm, kPreambleLength);
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Radio init"), state);
#if TBEAM_TEST_STAGE >= 3
  radio.setPacketReceivedAction(onPacketReceived);
#endif
  Serial.println(F("Radio ready at 915 MHz"));
#endif
  Serial.println(F("Setup complete"));
}

void loop() {
#if TBEAM_TEST_STAGE >= 3
  String ping = "PING:" + String(sequence++);
  Serial.print(F("TX  "));
  Serial.println(ping);

  int16_t state = radio.transmit(ping);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("TX failed, code "));
    Serial.println(state);
    delay(2000);
    return;
  }

  packetReceived = false;
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) stopWithError(F("Start receive"), state);

  const uint32_t deadline = millis() + 1500;
  while (!packetReceived && static_cast<int32_t>(deadline - millis()) > 0) {
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
    } else {
      Serial.print(F("RX read failed, code "));
      Serial.println(state);
    }
  } else {
    radio.standby();
    Serial.println(F("RX  timeout"));
  }

  delay(2000);
#else
  // Repeat so opening the USB monitor after boot still shows useful output.
  static uint32_t heartbeat = 0;
  Serial.print(F("Heartbeat "));
  Serial.print(heartbeat++);
  Serial.print(F(" | uptime_ms="));
  Serial.print(millis());
  Serial.print(F(" | stage="));
  Serial.println(TBEAM_TEST_STAGE);
  delay(1000);
#endif
}
