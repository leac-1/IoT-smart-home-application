#pragma once
#include <stdint.h>
#include <stdbool.h>

struct NodeConfig{
    uint8_t nodeId; // unique ID assigned by the gateway
    unsigned long sleepMs; // sleep interval in miliseconds
    uint8_t tdmaSlot; // TDMA slot assigned by gateway
};

// Listen for beacon from gateway
// Returns true if a beacon was received within the timeout.
// TODO: implement using Serial2 / RN2483
bool waitForBeacon(unsigned long timeoutMs);

// Send a join request (0x0F) to the gateway using broadcast address 0xFF.
// Returns true if the request was sent successfully.
// TODO: implement using build_packet(0x0F, ...)
bool sendJoinRequest();

// Listen for a join accept (0x12) from the gateway.
// Populates NodeConfig with assigned ID, sleep time, and TDMA slot.
// Returns true if a valid join accept was received.
// TODO: implement — parse incoming packet, extract NodeConfig fields
bool receiveJoinAccept(NodeConfig& config);

// Full join sequence — calls waitForBeacon, sendJoinRequest, receiveJoinAccept.
// Retries up to maxRetries times before giving up.
// Returns true if node successfully joined.
bool joinNetwork(NodeConfig& config, int maxRetries = 5);