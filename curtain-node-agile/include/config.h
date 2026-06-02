#pragma once
#include <stdint.h>

extern uint8_t assignedNodeId;

// Curtain state thresholds (temperature in Celsius)
constexpr float HOT_THRESHOLD_C = 26.0;
constexpr float COLD_THRESHOLD_C = 20.0;

// Curtain state definitions
enum CurtainState {
    CURTAIN_UNKNOWN = 0,
    CURTAIN_OPEN = 1,
    CURTAIN_CLOSED = 2
};

// Optional: Send status periodically even if state doesn't change
constexpr unsigned long STATUS_INTERVAL_MS = 300000; // 5 minutes

// TDMA downlink mode is the default for Version 2; the always-on receive loop remains as a fallback.
constexpr bool CURTAIN_USE_TDMA_DOWNLINK = true;
constexpr unsigned long DOWNLINK_WAKE_GUARD_MS = 200;
constexpr unsigned long MIN_DEEP_SLEEP_MS = 10000;
