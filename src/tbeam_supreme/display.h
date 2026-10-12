#pragma once
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include "LinkScreen.h"

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
}  // namespace
