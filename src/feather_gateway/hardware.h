#pragma once
#include <Adafruit_ST7789.h>
#include <Adafruit_MAX1704X.h>
#include <Adafruit_LC709203F.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include "LinkScreen.h"
#include "gps.h"
#include "GpsPayload.h"
#include "GeoDistance.h"

namespace {
Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 frame(240, 135);
LinkScreen screen;
linkProtocol::GpsPayload tbeamPosition;
bool tbeamPositionReceived = false;
uint32_t tbeamPositionUpdated = 0;

bool tbeamDistanceAvailable() {
  return !strcmp(gpsStatus(), "FIX") && tbeamPositionReceived &&
         tbeamPosition.status == 1 && !screen.peerSilent &&
         millis() - tbeamPositionUpdated <= 6000;
}

double distanceToTbeam() {
  return linkProtocol::distanceMeters(gps.latitudeDegrees, gps.longitudeDegrees,
                                      tbeamPosition.latitude, tbeamPosition.longitude);
}
Adafruit_MAX17048 maxBattery;
Adafruit_LC709203F lcBattery;
enum class BatteryGauge { None, Max, Lc };
BatteryGauge batteryGauge = BatteryGauge::None;
bool batteryValid = false;
float batteryPercent = 0, batteryVoltage = 0;
constexpr uint32_t kBootAnimationMs = 5000;
uint32_t bootAnimationStarted = 0;
bool bootAnimationPending = true;

void drawBootAnimation(uint32_t elapsed) {
  frame.fillScreen(ST77XX_BLACK);
  frame.setTextWrap(false);
  frame.setTextSize(2);
  frame.setTextColor(ST77XX_WHITE);
  frame.setCursor(54, 12);
  frame.print("BIKE FINDER");
  constexpr int cx = 120, cy = 65;
  frame.drawCircle(cx, cy, 25, 0x2104);
  frame.drawCircle(cx, cy, 13, 0x2104);
  frame.drawFastHLine(cx - 25, cy, 51, 0x2104);
  frame.drawFastVLine(cx, cy - 25, 51, 0x2104);
  const double angle = elapsed * 0.004;
  frame.drawLine(cx, cy, cx + (int)(sin(angle) * 24),
                 cy - (int)(cos(angle) * 24), ST77XX_CYAN);
  frame.drawCircle(cx, cy, 2 + (elapsed / 80) % 23, 0x0410);
  frame.fillCircle(cx + 12, cy - 15, (elapsed / 300) % 2 ? 3 : 2, ST77XX_GREEN);
  frame.fillCircle(cx, cy, 3, ST77XX_CYAN);
  frame.setTextSize(1);
  frame.setTextColor(0xC618);
  frame.setCursor(57, 101);
  frame.print("Starting your finder");
  for (unsigned int i = 0; i < (elapsed / 400) % 4; ++i) frame.print('.');
  frame.drawRoundRect(20, 119, 200, 7, 3, 0x2104);
  const int progress = (196UL * elapsed) / kBootAnimationMs;
  if (progress > 0) frame.fillRect(22, 121, progress, 3, ST77XX_CYAN);
}

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
void drawStatusBar() {
  const uint16_t muted = 0x8410;
  const bool fix = !strcmp(gpsStatus(), "FIX");
  const bool radioError = screen.status.indexOf("ERROR") >= 0;
  const bool radioLive = screen.hasSignal && !screen.peerSilent &&
                         millis() - screen.lastPacket <= 6000 && !radioError;
  frame.fillRect(0, 0, 240, 23, 0x1082);
  frame.setTextSize(1);
  frame.setTextColor(ST77XX_WHITE);
  frame.setCursor(8, 8);
  frame.print("FIND T-BEAM");
  // GPS target icon, radio bars, and battery form the top-right status cluster.
  const uint16_t gpsColor = fix ? ST77XX_GREEN : ST77XX_ORANGE;
  frame.drawCircle(125, 10, 4, gpsColor);
  frame.drawFastHLine(118, 10, 15, gpsColor);
  frame.drawFastVLine(125, 3, 15, gpsColor);
  frame.setTextColor(gpsColor);
  frame.setCursor(137, 7); frame.print("GPS");
  const uint16_t radioColor = radioLive ? ST77XX_CYAN : (radioError ? ST77XX_RED : muted);
  const int bars = radioLive ? constrain((int)((screen.rssi + 120) / 12), 1, 4) : 0;
  for (int i = 0; i < 4; ++i) {
    const int height = 3 + i * 3;
    if (i < bars) frame.fillRect(160 + i * 4, 17 - height, 3, height, radioColor);
    else frame.drawRect(160 + i * 4, 17 - height, 3, height, radioColor);
  }
  const uint16_t batteryColor = !batteryValid ? muted :
      (batteryPercent < 20 ? ST77XX_RED : ST77XX_GREEN);
  frame.drawRect(182, 5, 20, 11, batteryColor);
  frame.fillRect(202, 8, 2, 5, batteryColor);
  if (batteryValid) frame.fillRect(184, 7, (int)(16 * batteryPercent / 100), 7, batteryColor);
  frame.setTextColor(batteryColor);
  frame.setCursor(209, 7);
  if (batteryValid) frame.printf("%.0f%%", batteryPercent);
  else frame.print("--");
}

void drawCompass(bool available, double bearing, uint16_t ink) {
  constexpr int cx = 191, cy = 77, radius = 29;
  frame.drawCircle(cx, cy, radius, 0x8410);
  frame.setTextSize(1);
  frame.setTextColor(ST77XX_WHITE);
  frame.setCursor(cx - 2, cy - radius - 11); frame.print("N");
  frame.setCursor(cx - 2, cy + radius + 4); frame.print("S");
  frame.setCursor(cx - radius - 10, cy - 3); frame.print("W");
  frame.setCursor(cx + radius + 5, cy - 3); frame.print("E");
  if (!available) {
    frame.setCursor(cx - 2, cy - 3); frame.print("?");
    return;
  }
  const double angle = bearing * 0.017453292519943295;
  const double dx = sin(angle), dy = -cos(angle);
  frame.drawLine(cx - (int)(dx * 16), cy - (int)(dy * 16),
                 cx + (int)(dx * 18), cy + (int)(dy * 18), ink);
  frame.fillTriangle(cx + (int)(dx * 23), cy + (int)(dy * 23),
                     cx + (int)(dx * 10 - dy * 7), cy + (int)(dy * 10 + dx * 7),
                     cx + (int)(dx * 10 + dy * 7), cy + (int)(dy * 10 - dx * 7), ink);
  frame.fillCircle(cx, cy, 2, ink);
}

void updateScreen() {
  static uint32_t previous = 0;
  if (millis() - previous < 120) return;
  previous = millis();
  if (!frame.getBuffer()) return;
  if (bootAnimationPending) {
    bootAnimationStarted = millis();
    bootAnimationPending = false;
  }
  const uint32_t bootElapsed = millis() - bootAnimationStarted;
  if (bootElapsed < kBootAnimationMs) {
    drawBootAnimation(bootElapsed);
    display.drawRGBBitmap(0, 0, frame.getBuffer(), 240, 135);
    return;
  }
  frame.fillScreen(ST77XX_BLACK);
  frame.setTextWrap(false);
  drawStatusBar();
  const bool available = tbeamDistanceAvailable();
  const double distance = available ? distanceToTbeam() : 0;
  // GPS error makes bearings unstable at very short separations.
  const bool directionAvailable = available && distance >= 10;
  const double bearing = directionAvailable ? linkProtocol::bearingDegrees(
      gps.latitudeDegrees, gps.longitudeDegrees,
      tbeamPosition.latitude, tbeamPosition.longitude) : 0;
  const uint16_t ink = available && distance <= linkProtocol::kCloseMeters ? ST77XX_GREEN : ST77XX_CYAN;
  frame.setTextSize(1);
  frame.setTextColor(0xC618);
  frame.setCursor(10, 31); frame.print("APPROX. DISTANCE");
  frame.setTextSize(3);
  frame.setTextColor(available ? ink : ST77XX_ORANGE);
  frame.setCursor(10, 46);
  if (!available) frame.print("--");
  else {
    char distanceText[24];
    if (distance < 1000) snprintf(distanceText, sizeof(distanceText), "%.0f m", distance);
    else snprintf(distanceText, sizeof(distanceText), "%.1f km", distance / 1000);
    if (strlen(distanceText) > 8) frame.setTextSize(2);
    frame.print(distanceText);
  }
  frame.setTextSize(2);
  frame.setCursor(10, 83);
  if (directionAvailable) frame.printf("%s %.0f", linkProtocol::compassDirection(bearing), bearing);
  else if (available) frame.print("NEARBY");
  else frame.print("WAITING");
  drawCompass(directionAvailable, bearing, ink);
  frame.setTextSize(1);
  frame.setTextColor(0xC618);
  frame.setCursor(10, 123);
  if (screen.status.indexOf("ERROR") >= 0) frame.print(screen.status.substring(0, 36));
  else if (screen.peerSilent) frame.print("T-Beam signal lost");
  else if (!strcmp(gpsStatus(), "FIX") && !available) frame.print("Waiting for T-Beam GPS");
  else if (!available) frame.print("Waiting for your GPS fix");
  else if (!directionAvailable) frame.print("Nearby - look around");
  else frame.print("NORTH-UP | bearing to T-Beam");
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
