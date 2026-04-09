#pragma once
#include <stdint.h>

// Defines the incoming packet from the gateway
// To be agreed upon with protocol designer

struct IncomingPacket {
    uint8_t nodeId;     // which node this packet is addressed to
    uint8_t command;    // 0x00 = off, 0x01 = on
};

// Implemented in lora.cpp
bool receivePacket(IncomingPacket& packet);