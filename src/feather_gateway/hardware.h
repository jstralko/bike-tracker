#pragma once
#include <Adafruit_ST7789.h>
#include <Adafruit_MAX1704X.h>
#include <Adafruit_LC709203F.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include "LinkScreen.h"

namespace {
Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 frame(240, 135);
LinkScreen screen;
Adafruit_MAX17048 maxBattery;
Adafruit_LC709203F lcBattery;
enum class BatteryGauge { None, Max, Lc };
BatteryGauge batteryGauge = BatteryGauge::None;
bool batteryValid = false;
float batteryPercent = 0, batteryVoltage = 0;

void beginBattery() {
  Wire.begin(SDA, SCL);
  Wire.setTimeOut(25);
  if (maxBattery.begin(&Wire)) {
    batteryGauge = BatteryGauge::Max;
    Serial.println(F("Battery gauge: MAX17048"));
  } else if (lcBattery.begin(&Wire)) {
    batteryGauge = BatteryGauge::Lc;
    lcBattery.setThermistorB(3950);
    // Adafruit's default profile. Adjust for your pack if using this older gauge.
    lcBattery.setPackSize(LC709203F_APA_500MAH);
    Serial.println(F("Battery gauge: LC709203F (500 mAh profile)"));
  } else {
    Serial.println(F("Battery gauge unavailable; battery display shows --"));
  }
}

void updateBattery() {
  static uint32_t previous = 0;
  static bool sampled = false;
  if (sampled && millis() - previous < 5000) return;
  sampled = true;
  previous = millis();
  if (batteryGauge == BatteryGauge::None) return;
  batteryVoltage = batteryGauge == BatteryGauge::Max ? maxBattery.cellVoltage() : lcBattery.cellVoltage();
  const float percent = batteryGauge == BatteryGauge::Max ? maxBattery.cellPercent() : lcBattery.cellPercent();
  batteryValid = isfinite(batteryVoltage) && isfinite(percent) &&
                 batteryVoltage >= 2.5f && batteryVoltage <= 4.5f && percent >= 0;
  if (batteryValid) batteryPercent = constrain(percent, 0.0f, 100.0f);
}
void updateScreen() {
  static uint32_t previous = 0;
  if (millis() - previous < 120) return;
  previous = millis();
  if (!frame.getBuffer()) return;
  frame.fillScreen(0);
  screen.draw(frame, "FEATHER / GATEWAY", screen.peerSilent ? ST77XX_ORANGE : ST77XX_CYAN);
  const uint16_t batteryColor = batteryValid && batteryPercent < 20 ? ST77XX_RED : ST77XX_GREEN;
  frame.setTextSize(1);
  frame.setTextColor(batteryValid ? batteryColor : ST77XX_WHITE);
  frame.setCursor(0, 110);
  if (batteryValid) frame.printf("BAT %.0f%%  %.2fV", batteryPercent, batteryVoltage);
  else frame.print("BAT -- (unavailable)");
  frame.drawRect(207, 110, 28, 9, batteryColor);
  frame.fillRect(235, 112, 2, 5, batteryColor);
  if (batteryValid) frame.fillRect(209, 112, (int)(24 * batteryPercent / 100), 5, batteryColor);
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
}  // namespace
