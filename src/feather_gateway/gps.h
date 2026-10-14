#pragma once
#include <Adafruit_GPS.h>
#include "config.h"

namespace {
// GPS TX -> Feather RX/GPIO2; GPS RX -> Feather TX/GPIO1.
Adafruit_GPS gps(&Serial1);
uint32_t gpsLastByte = 0, gpsLastNavigation = 0, gpsParseErrors = 0;
bool gpsHasBytes = false, gpsHasNavigation = false;

void beginGps() {
  Serial1.setRxBufferSize(1024);
  Serial1.setPins(boardConfig::kGpsRx, boardConfig::kGpsTx);
  gps.begin(9600);
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  Serial.println(F("GPS UART started: 9600 baud | RX=2 TX=1 | waiting for NMEA"));
}

const char *gpsStatus() {
  if (!gpsHasBytes || millis() - gpsLastByte > boardConfig::kGpsStaleMs) return "NO DATA";
  if (!gpsHasNavigation || millis() - gpsLastNavigation > boardConfig::kGpsStaleMs) return "NO NMEA";
  return gps.fix ? "FIX" : "NO FIX";
}

void pollGps() {
  // Parse each completed sentence before reading the next one.
  while (Serial1.available()) {
    gps.read();
    gpsHasBytes = true;
    gpsLastByte = millis();
    if (!gps.newNMEAreceived()) continue;
    char *sentence = gps.lastNMEA();
    const bool navigation = strlen(sentence) >= 6 &&
        (!strncmp(sentence + 3, "RMC", 3) || !strncmp(sentence + 3, "GGA", 3));
    if (gps.parse(sentence)) {
      if (navigation) {
        gpsHasNavigation = true;
        gpsLastNavigation = millis();
      }
    } else {
      ++gpsParseErrors;
    }
  }
}

void logGps() {
  static uint32_t previous = 0;
  if (millis() - previous < 2000) return;
  previous = millis();
  Serial.printf("GPS %s | satellites=%u | parse_errors=%lu", gpsStatus(),
                static_cast<unsigned int>(gps.satellites),
                static_cast<unsigned long>(gpsParseErrors));
  if (!strcmp(gpsStatus(), "FIX")) {
    Serial.printf(" | lat=%.6f lon=%.6f | altitude=%.1fm", gps.latitudeDegrees,
                  gps.longitudeDegrees, gps.altitude);
  }
  Serial.println();
}
}  // namespace
