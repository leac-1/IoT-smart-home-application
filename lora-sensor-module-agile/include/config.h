#pragma once
#include <stdint.h>

constexpr unsigned long SLEEP_SECONDS = 600;
constexpr bool USE_DHT = true;
constexpr bool USE_LDR = true;

extern uint8_t assignedNodeId;
extern unsigned long assignedSleepSeconds;