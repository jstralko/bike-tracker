#pragma once
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
         !screen.peerSilent && millis() - remoteGpsUpdated <= 6000;
}
double featherDistance() {
  return linkProtocol::distanceMeters(localGps.location.lat(), localGps.location.lng(),
                                      remoteGps.latitude, remoteGps.longitude);
}
void updateScreen() {
  static uint32_t previous = 0;
  if (!displayReady || millis() - previous < 150) return;
  previous = millis();
  display.clearDisplay();
  screen.draw(display, "T-BEAM / TRACKER", SH110X_WHITE);
  // Cycle radio, remote coordinates, and proximity pages.
  const unsigned int page = (millis() / 4000) % 3;
  if (page == 1) {
    display.fillRect(0, 32, 128, 20, SH110X_BLACK);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 32);
    const bool fresh = remoteGpsReceived && !screen.peerSilent &&
                       millis() - remoteGpsUpdated <= 6000;
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
}  // namespace
