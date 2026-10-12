#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// State is updated from the main loop, never from a radio interrupt.
struct LinkScreen {
  String status = "BOOTING";
  String packet = "Waiting for packets";
  uint32_t tx = 0, rx = 0, misses = 0;
  float rssi = 0, snr = 0;
  bool hasSignal = false;
  uint32_t lastPacket = 0;

  void received(const String &message, float strength, float noise) {
    packet = message;
    rssi = strength;
    snr = noise;
    hasSignal = true;
    lastPacket = millis();
    ++rx;
  }

  void draw(Adafruit_GFX &g, const char *title, uint16_t ink) {
    const int w = g.width();
    const bool large = g.height() > 100;
    const int scale = large ? 2 : 1;
    g.setTextWrap(false);
    g.setTextColor(ink);
    g.setTextSize(scale);
    g.setCursor(0, 0); g.print(title);
    const int y = large ? 23 : 12;
    g.drawFastHLine(0, y - 3, w, ink);
    g.setCursor(0, y); g.print(status.substring(0, large ? 18 : 19));
    g.setCursor(0, y + 10 * scale);
    g.printf("TX:%lu RX:%lu", (unsigned long)tx, (unsigned long)rx);
    g.setTextSize(1);
    g.setCursor(0, y + 20 * scale);
    if (hasSignal) g.printf("RSSI %.0f dBm  SNR %.1f", rssi, snr);
    else g.print("915 MHz | awaiting link");
    g.setCursor(0, y + 20 * scale + 10);
    g.print(packet.substring(0, w / 6));
    if (large) {
      g.setCursor(0, 98);
      g.printf("MISS:%lu  LAST:", (unsigned long)misses);
      if (hasSignal) g.printf("%lus", (unsigned long)((millis()-lastPacket)/1000));
      else g.print("--");
    }
    // A moving packet dot travels between two nodes; bars show last RSSI.
    const int bottom = g.height() - 6;
    g.drawCircle(4, bottom, 3, ink);
    g.drawCircle(w - 38, bottom, 3, ink);
    g.drawFastHLine(8, bottom, w - 50, ink);
    const int travel = (millis() / 45) % (w - 50);
    g.fillCircle(8 + travel, bottom, 2, ink);
    const int bars = hasSignal ? constrain((int)((rssi + 120) / 12), 0, 5) : 0;
    for (int i = 0; i < 5; ++i) {
      int height = 2 + i * 2;
      if (i < bars) g.fillRect(w - 28 + i * 5, g.height()-height, 3, height, ink);
      else g.drawRect(w - 28 + i * 5, g.height()-height, 3, height, ink);
    }
  }
};
