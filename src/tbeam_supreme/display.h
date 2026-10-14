#pragma once
#include "config.h"
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include "LinkScreen.h"
#include "GpsPayload.h"
#include "GeoDistance.h"
#include "gps.h"

namespace {
Adafruit_SH1106G display(128, 64, &Wire, -1);
LinkScreen screen;
bool displayReady = false;
linkProtocol::GpsPayload remoteGps;
bool remoteGpsReceived = false;
uint32_t remoteGpsUpdated = 0;
bool proximityAvailable() {
  return localGpsFresh() && remoteGpsReceived && remoteGps.status == 1 &&
         !screen.peerSilent && millis() - remoteGpsUpdated <= boardConfig::kRemotePositionStaleMs;
}
double featherDistance() {
  return linkProtocol::distanceMeters(localGps.location.lat(), localGps.location.lng(),
                                      remoteGps.latitude, remoteGps.longitude);
}
void drawDashboard() {
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("T-BEAM");
  // Radio bars and a local GPS target stay visible across all three pages.
  const bool radioLive = screen.hasSignal && !screen.peerSilent &&
                         millis() - screen.lastPacket <= boardConfig::kRemotePositionStaleMs &&
                         screen.status.indexOf("ERROR") < 0;
  const int bars = radioLive ? constrain((int)((screen.rssi + 120) / 12), 1, 4) : 0;
  for (int i = 0; i < 4; ++i) {
    const int height = 2 + i * 2;
    if (i < bars) display.fillRect(94 + i * 4, 8 - height, 3, height, SH110X_WHITE);
    else display.drawRect(94 + i * 4, 8 - height, 3, height, SH110X_WHITE);
  }
  display.drawCircle(120, 4, 3, SH110X_WHITE);
  display.drawFastHLine(115, 4, 11, SH110X_WHITE);
  display.drawFastVLine(120, 0, 9, SH110X_WHITE);
  if (localGpsFresh()) display.fillCircle(120, 4, 1, SH110X_WHITE);
  else display.drawLine(116, 0, 124, 8, SH110X_WHITE);
  display.drawFastHLine(0, 9, 128, SH110X_WHITE);
  const bool invertWarning = screen.peerSilent && (millis() / 500) % 2 == 0;
  if (invertWarning) {
    display.fillRect(0, 11, 128, 10, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  }
  display.setCursor(0, 12);
  display.print(screen.status.substring(0, 19));
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 32);
  if (screen.hasSignal) display.printf("RSSI %.0f dBm SNR %.1f", screen.rssi, screen.snr);
  else display.print("915 MHz | waiting");
  display.setCursor(0, 42);
  display.print(screen.packet.substring(0, 21));
  // Keep the link's moving packet dot, with signal bars now in the header.
  display.drawCircle(4, 58, 3, SH110X_WHITE);
  display.drawCircle(124, 58, 3, SH110X_WHITE);
  display.drawFastHLine(8, 58, 112, SH110X_WHITE);
  if (!screen.peerSilent) display.fillCircle(8 + (millis() / 45) % 112, 58, 2, SH110X_WHITE);
}
void updateScreen() {
  static uint32_t previous = 0;
  if (!displayReady || millis() - previous < boardConfig::kScreenRefreshMs) return;
  previous = millis();
  display.clearDisplay();
  drawDashboard();
  // Cycle radio, remote coordinates, and proximity pages.
  const unsigned int page = (millis() / boardConfig::kScreenPageMs) % 3;
  if (page == 1) {
    display.fillRect(0, 32, 128, 20, SH110X_BLACK);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 32);
    const bool fresh = remoteGpsReceived && !screen.peerSilent &&
                       millis() - remoteGpsUpdated <= boardConfig::kRemotePositionStaleMs;
    if (fresh && remoteGps.status == 1) {
      display.printf("LAT %.6f", remoteGps.latitude);
      display.setCursor(0, 42);
      display.printf("LON %.6f", remoteGps.longitude);
    } else {
      display.print(fresh && remoteGps.status == 0 ? "GPS NO FIX" : "GPS UNAVAILABLE");
      display.setCursor(0, 42);
      if (fresh) display.printf("SAT:%u", remoteGps.satellites);
      else display.print("Waiting for fresh GPS");
    }
  }
  if (page == 2) {
    display.fillRect(0, 32, 128, 20, SH110X_BLACK);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 32);
    if (proximityAvailable()) {
      const double distance = featherDistance();
      display.printf("FEATHER %s", distance <= linkProtocol::kCloseMeters ? "CLOSE" : "FAR");
      display.setCursor(0, 42);
      display.printf("DIST %.1f m", distance);
    } else {
      display.print("DIST --");
      display.setCursor(0, 42);
      if (!localGpsFresh()) display.printf("LOCAL GPS %s", localGpsStatus());
      else display.print("REMOTE GPS WAITING");
    }
  }
  display.display();
}
void screenDelay(uint32_t duration) {
  uint32_t started = millis();
  while (millis() - started < duration) {
    pollLocalGps();
    updateScreen();
    delay(1);
  }
}
void beginScreen() {
  Wire.begin(boardConfig::kDisplaySda, boardConfig::kDisplayScl);
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
}  // namespace
