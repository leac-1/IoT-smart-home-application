#pragma once
#include <stdint.h>

constexpr bool USE_DHT = true;
constexpr bool USE_LDR = true;

extern uint8_t assignedNodeId;
extern unsigned long assignedSleepSeconds;

#ifndef NODE_ID
#define NODE_ID 1
#endif