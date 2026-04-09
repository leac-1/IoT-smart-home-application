#include <Arduino.h>
#include "pins.h"
#include "packet.h"

// Stub — custom protocol to be implemented by protocol designer.
// SPI pins are defined in pins.h.
// Fill in receivePacket() to populate an IncomingPacket and return true
// when a valid packet addressed to this node is received.

void initLoRa() {
    // TODO: initialise SPI and configure LoRa module
}

bool receivePacket(IncomingPacket& packet) {
    // TODO: check for incoming packet, decrypt, validate
    // populate packet.nodeId and packet.command
    // return true if a valid packet was received, false otherwise
    return false;
}