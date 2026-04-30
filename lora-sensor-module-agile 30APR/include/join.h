#pragma once
#include <stdint.h>
#include <stdbool.h>

struct NodeConfig {
    uint8_t nodeId;
    unsigned long sleepMs;
    uint8_t tdmaSlot;
};

bool waitForBeacon(unsigned long timeoutMs);
bool sendJoinRequest();
bool receiveJoinAccept(NodeConfig& config);
bool joinNetwork(NodeConfig& config, int maxRetries = 5);