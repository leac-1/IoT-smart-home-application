#pragma once
#include <stdint.h>
#include <stdbool.h>

struct NodeConfig {
    uint8_t nodeId;
    unsigned long sleepMs;
    uint8_t tdmaSlot;
    uint32_t cycleTime;
    uint8_t slotDuration;
    uint8_t currentSlotCount;
};

bool waitForBeacon(unsigned long timeoutMs);
bool waitForBeaconWithTDMA(NodeConfig& config);
bool sendJoinRequest();
bool receiveJoinAccept(NodeConfig& config);
bool joinNetwork(NodeConfig& config, int maxRetries = 5);