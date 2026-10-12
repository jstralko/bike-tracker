#include <Arduino.h>

// ESP32-S3 bring-up stages; the legacy M0 gateway is kept below.
#ifndef FEATHER_MAIN_BOARD_TEST
#define FEATHER_MAIN_BOARD_TEST 1
#endif

#if FEATHER_MAIN_BOARD_TEST
#include <RadioLib.h>
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include "LinkScreen.h"

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

namespace {
Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 frame(240, 135);
LinkScreen screen;
void updateScreen() {
  static uint32_t previous = 0;
  if (millis() - previous < 120) return;
  previous = millis();
  if (!frame.getBuffer()) return;
  frame.fillScreen(0);
  screen.draw(frame, "FEATHER / GATEWAY", ST77XX_CYAN);
  display.drawRGBBitmap(0, 0, frame.getBuffer(), 240, 135);
}
void beginScreen() {
  pinMode(TFT_I2C_POWER, OUTPUT); digitalWrite(TFT_I2C_POWER, HIGH);
  pinMode(FEATHER_RADIO_CS, OUTPUT); digitalWrite(FEATHER_RADIO_CS, HIGH);
  SPI.begin(SCK, MISO, MOSI);
  display.init(135, 240); display.setRotation(3);
  pinMode(TFT_BACKLITE, OUTPUT); digitalWrite(TFT_BACKLITE, HIGH);
  display.fillScreen(ST77XX_BLACK);
}
#if FEATHER_TEST_STAGE >= 1
SX1276 radio = new Module(FEATHER_RADIO_CS, FEATHER_RADIO_IRQ,
                          FEATHER_RADIO_RST, RADIOLIB_NC);
int16_t radioStatus = RADIOLIB_ERR_NONE;
bool radioReady = false;
#if FEATHER_TEST_STAGE == 2
volatile bool packetReceived = false;
uint32_t pongsSent = 0;

void ARDUINO_ISR_ATTR onPacketReceived() { packetReceived = true; }

void listen() {
  packetReceived = false;
  radioStatus = radio.startReceive();
  radioReady = radioStatus == RADIOLIB_ERR_NONE;
  screen.status = radioReady ? "LISTENING" : "RX ERROR " + String(radioStatus);
  if (!radioReady) Serial.printf("Start receive failed, code %d\n", radioStatus);
}

void pollRadio() {
  if (!radioReady || !packetReceived) return;
  packetReceived = false;
  String message;
  int16_t state = radio.readData(message);
  if (state == RADIOLIB_ERR_NONE) {
    screen.received(message, radio.getRSSI(), radio.getSNR());
    Serial.printf("RX  %s  RSSI=%.2f dBm  SNR=%.2f dB\n",
                  message.c_str(), radio.getRSSI(), radio.getSNR());
    bool validPing = message.startsWith("PING:") && message.length() > 5;
    for (unsigned int i = 5; validPing && i < message.length(); ++i) {
      validPing = message[i] >= '0' && message[i] <= '9';
    }
    if (validPing) {
      String pong = "PONG:" + message.substring(5);
      delay(50);  // Give the T-Beam time to switch from TX to RX.
      state = radio.transmit(pong);
      if (state == RADIOLIB_ERR_NONE) {
        ++pongsSent;
        ++screen.tx;
        screen.packet = pong;
        Serial.printf("TX  %s | pongs=%lu\n", pong.c_str(),
                      static_cast<unsigned long>(pongsSent));
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
  radioStatus = radio.begin(915.0, 125.0, 9, 7, 0x12, 17, 8);
  if (radioStatus == RADIOLIB_ERR_NONE) radioStatus = radio.setCRC(true);
  if (radioStatus == RADIOLIB_ERR_NONE) radioStatus = radio.standby();
  radioReady = radioStatus == RADIOLIB_ERR_NONE;
  screen.status = radioReady ? "RADIO READY" : "INIT ERROR " + String(radioStatus);
  if (radioReady) {
#if FEATHER_TEST_STAGE == 2
    radio.setPacketReceivedAction(onPacketReceived);
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
  Serial.println(F("Feather ESP32-S3 TFT bring-up"));
#if FEATHER_TEST_STAGE >= 1
  initializeRadio();
#endif
  Serial.println(F("Setup complete"));
}

void loop() {
  updateScreen();
#if FEATHER_TEST_STAGE == 2
  pollRadio();
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat < 1000) {
    delay(1);
    return;
  }
  lastHeartbeat = millis();
#endif
  static uint32_t heartbeat = 0;
  //Serial.print(F("Heartbeat "));
  //Serial.print(heartbeat++);
  //Serial.print(F(" | uptime_ms="));
  //Serial.print(millis());
#if FEATHER_TEST_STAGE >= 1
#if FEATHER_TEST_STAGE == 2
  //Serial.print(F(" | test=ping-pong | radio="));
#else
  Serial.print(F(" | test=radio-init | radio="));
#endif
  //Serial.print(radioReady ? F("ready") : F("failed"));
  //Serial.print(F(" | code="));
  //Serial.println(radioStatus);
#else
  Serial.println(F(" | test=serial-only"));
#endif
#if FEATHER_TEST_STAGE != 2
  delay(1000);
#endif
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
