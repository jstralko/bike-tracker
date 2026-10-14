#pragma once
#include <Arduino.h>

// Start at 0 and increase one stage at a time; see TODO.md.
// 0 = serial only, 1 = PMU power, 2 = radio init, 3 = ping/pong.
// The preprocessor comments out hardware code for the inactive stages.
#ifndef TBEAM_TEST_STAGE
#define TBEAM_TEST_STAGE 3
#endif

#if TBEAM_TEST_STAGE < 0 || TBEAM_TEST_STAGE > 3
#error "TBEAM_TEST_STAGE must be between 0 and 3"
#endif

#define XPOWERS_CHIP_AXP2101

namespace boardConfig {
constexpr int kRadioSck = 12;
constexpr int kRadioMiso = 13;
constexpr int kRadioMosi = 11;
constexpr int kRadioCs = 10;
constexpr int kRadioReset = 5;
constexpr int kRadioDio1 = 1;
constexpr int kRadioBusy = 4;
constexpr int kPmuSda = 42;
constexpr int kPmuScl = 41;
constexpr int kDisplaySda = 17;
constexpr int kDisplayScl = 18;
constexpr int kGpsRx = 9;
constexpr int kGpsTx = 8;
constexpr int kGpsWake = 7;
constexpr uint32_t kGpsStaleMs = 5000;
constexpr uint32_t kRemotePositionStaleMs = 6000;
constexpr uint32_t kScreenRefreshMs = 150;
constexpr uint32_t kBootScreenMs = 3000;
constexpr uint32_t kScreenPageMs = 4000;
}  // namespace boardConfig
