#pragma once
#include <TinyGPS++.h>

namespace {
TinyGPSPlus localGps;
bool localGpsStarted = false;
uint32_t localGpsLastByte = 0;
bool localGpsHasBytes = false;

void beginLocalGps() {
  // LilyGO L76K: ALDO4 powered by setup; GPIO7 high keeps GNSS awake.
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  Serial1.setRxBufferSize(1024);
  Serial1.begin(9600, SERIAL_8N1, 9, 8);
  localGpsStarted = true;
  Serial.println(F("T-Beam GPS UART started: 9600 baud | RX=9 TX=8"));
}

void pollLocalGps() {
  if (!localGpsStarted) return;
  while (Serial1.available()) {
    localGps.encode(Serial1.read());
    localGpsHasBytes = true;
    localGpsLastByte = millis();
  }
}

bool localGpsFresh() {
  return localGpsStarted && localGps.location.isValid() && localGps.location.age() <= 5000;
}

const char *localGpsStatus() {
  if (!localGpsHasBytes || millis() - localGpsLastByte > 5000) return "NO DATA";
  return localGpsFresh() ? "FIX" : "NO FIX";
}
}  // namespace
