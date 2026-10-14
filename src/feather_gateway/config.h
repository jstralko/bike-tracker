#pragma once
#include <Arduino.h>

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

namespace boardConfig {
constexpr int kGpsRx = 2;
constexpr int kGpsTx = 1;
constexpr uint32_t kGpsStaleMs = 5000;
constexpr uint32_t kBootAnimationMs = 5000;
constexpr uint32_t kBatteryRefreshMs = 5000;
constexpr uint32_t kScreenRefreshMs = 120;
constexpr uint32_t kRemotePositionStaleMs = 6000;
// Approximate RSSI categories for the last received LoRa packet, not link guarantees.
constexpr float kStrongRssiDbm = -80;
constexpr float kGoodRssiDbm = -100;
constexpr float kFairRssiDbm = -115;
}  // namespace boardConfig
