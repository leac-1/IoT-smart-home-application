#include <Arduino.h>
#include "join.h"
#include "packetBuilder.h"
#include "pins.h"

bool waitForBeacon(unsigned long timeoutMs) {
    // Listen on Serial2 for an incoming packet from the RN2483
    // RN2483 outputs "radio_rx  <hexstring>" when it receives a packet
    
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');
            Serial.println("RN2483: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                // Extract hex payload from response
                int startIdx = response.indexOf("radio_rx") + 9; // Skip "radio_rx  "
                String hexPayload = response.substring(startIdx);
                
                // Parse packet header — check byte 3 == 0x11 (Beacon)
                if (hexPayload.length() >= 8) { // At least 4 bytes (8 hex chars)
                    int packetType = strtol(hexPayload.substring(6, 8).c_str(), NULL, 16);
                    if (packetType == 0x11) {
                        Serial.println("Packet received — assuming beacon");
                        return true;
                    }
                }
            }
            // For now, treat any received packet as a beacon
            Serial.println("Packet received does not match beacon format has type" + String(packetType) + " — ignoring");
            return true;
        }
    }
        Serial.println("Beacon timeout");
        return false;
}

bool sendJoinRequest() {
    // Send a 0x0F JOIN request packet
    // Source is 0x00 (no ID assigned yet)
    // Destination is 0x00 (gateway)
    unsigned char dst = 0x00;
    unsigned char payload = 0x00; // 1 byte payload, nonce or 0x00 for now

    unsigned char* packet = build_packet(0x0F, &payload, &dst, 1);
    if (packet == NULL) {
        Serial.println("Failed to build join request packet");
        return false;
    }

    // Convert to hex string for RN2483
    size_t packet_len = 6 + 1 + 4 + 1; // header + payload + MIC + CRC
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }

    free(packet);

    // Transmit via RN2483
    Serial2.println("radio tx " + hexStr);

    String response = Serial2.readStringUntil('\n');
    Serial.println("RN2483: " + response);

    return response.indexOf("ok") >= 0;
}

bool receiveJoinAccept(NodeConfig& config) {
    Serial.println("Waiting for join accept...");
    unsigned long start = millis();
    const unsigned long timeout = 5000; // 5 second timeout

    while (millis() - start < timeout) {
        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');
            Serial.println("RN2483: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                // Extract hex payload
                    int startIdx = response.indexOf("radio_rx") + 9;
                    String hexPayload = response.substring(startIdx);

                if (hexPayload.length() >= 8) {
                    int packetType = strtol(hexPayload.substring(6, 8).c_str(), NULL, 16);
                    // Check for JOIN accept (0x12)
                    if (packetType == 0x12) {
                        // Extract node ID/address from byte 0 (chars 0-2)
                        config.nodeId = strtol(hexPayload.substring(0, 2).c_str(), NULL, 16);
                        // Extract TDMA slot from byte 1 (chars 2-4)
                        config.tdmaSlot = strtol(hexPayload.substring(2, 4).c_str(), NULL, 16);
                        Serial.printf("Join accepted! Node ID: %d, TDMA Slot: %d\n", config.nodeId, config.tdmaSlot);
                        return true;
                    }
                }
            }
        }
    }
    Serial.println("Join accept timeout");
    return false;
}

bool joinNetwork(NodeConfig& config, int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        Serial.printf("Join attempt %d of %d\n", attempt + 1, maxRetries);

        if (!waitForBeacon(10000)) {
            Serial.println("No beacon received, retrying...");
            continue;
        }

        if (!sendJoinRequest()) {
            Serial.println("Failed to send join request, retrying...");
            continue;
        }

        if (receiveJoinAccept(config)) {
            Serial.printf("Joined! Assigned ID: %d\n", config.nodeId);
            return true;
        }



        Serial.println("No join accept received, retrying...");
    }

    Serial.println("Failed to join network after max retries");
    return false;
}