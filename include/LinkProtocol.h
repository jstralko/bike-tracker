#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "GpsPayload.h"

// One shared layer for the wire protocol and settings used by both radios.
// Hardware construction, power, interrupts, and screen updates stay on each board.
namespace linkProtocol {
constexpr float kFrequencyMHz = 915.0;
constexpr float kBandwidthKHz = 125.0;
constexpr uint8_t kSpreadingFactor = 9;
constexpr uint8_t kCodingRate = 7;
constexpr uint8_t kSyncWord = 0x12;
constexpr int8_t kTxPowerDbm = 17;
constexpr uint16_t kPreambleLength = 8;
constexpr uint32_t kReplyDelayMs = 50;
constexpr uint32_t kPongTimeoutMs = 1500;
constexpr uint32_t kPingIntervalMs = 2000;
constexpr uint32_t kPeerTimeoutMs = 6000;

inline String ping(uint32_t sequence) { return "PING:" + String(sequence); }
inline String pongFor(const String &pingMessage) {
  const int separator = pingMessage.indexOf('|');
  return "PONG:" + pingMessage.substring(5, separator < 0 ? pingMessage.length() : separator);
}
inline String withGps(const String &packet, const GpsPayload &gps) {
  return packet + "|GPS:1," + String(gps.status) + "," +
         String(gps.satellites) + "," + String(gps.latitude, 6) + "," +
         String(gps.longitude, 6);
}
inline String gpsPongFor(const String &pingMessage, const GpsPayload &gps) {
  return withGps(pongFor(pingMessage), gps);
}
inline bool isPing(const String &message) {
  if (!message.startsWith("PING:") || message.length() <= 5) return false;
  const int separator = message.indexOf('|');
  const unsigned int end = separator < 0 ? message.length() : separator;
  if (end <= 5) return false;
  for (unsigned int i = 5; i < end; ++i) {
    if (message[i] < '0' || message[i] > '9') return false;
  }
  if (separator < 0) return true;
  GpsPayload position;
  return parseGpsPong(message.c_str(), message.substring(0, end).c_str(), position);
}

template <typename Radio>
int16_t initializeRadio(Radio &radio) {
  int16_t state = radio.begin(kFrequencyMHz, kBandwidthKHz, kSpreadingFactor,
                             kCodingRate, kSyncWord, kTxPowerDbm, kPreambleLength);
  return state == RADIOLIB_ERR_NONE ? radio.setCRC(true) : state;
}

template <typename Radio>
void logReceived(Radio &radio, const String &message) {
  Serial.printf("RX  %s  RSSI=%.2f dBm  SNR=%.2f dB\n",
                message.c_str(), radio.getRSSI(), radio.getSNR());
}
}  // namespace linkProtocol
