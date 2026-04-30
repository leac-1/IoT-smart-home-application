#include <Arduino.h>
#include "join.h"
#include "packetBuilder.h"
#include "pins.h"
#include "security.h"

bool waitForBeacon(unsigned long timeoutMs) {
    unsigned long start = millis();

    Serial2.print("radio rx 0\r\n");
    delay(500);

    while (Serial2.available()) {
        Serial2.readStringUntil('\n');
    }

    while (millis() - start < timeoutMs) {
        yield();
        delay(1);
        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');

            if (response == "ok" || response == "ok\r") continue;

            Serial.println("RN2483: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                int startIdx = response.indexOf("radio_rx") + 9;
                String hexPayload = response.substring(startIdx);

                if (hexPayload.length() >= 8) {
                    int packetType = strtol(hexPayload.substring(5, 7).c_str(), NULL, 16);
                    if (packetType == 0x11) {
                        Serial.println("Beacon received");
                        return true;
                    } else {
                        Serial.println("Packet received but not a beacon, type: " + String(packetType, HEX) + " — ignoring");
                    }
                }
            }
        }
    }

    Serial.println("Beacon timeout");
    return false;
}

bool sendJoinRequest() {
    Serial2.print("radio rxstop\r\n");
    delay(500);
    Serial2.readStringUntil('\n');
    Serial2.readStringUntil('\n');

    unsigned char dst = 0x00;
    unsigned char payload = 0x00;

    unsigned char* packet = build_packet(0x0F, &payload, &dst, 1);
    if (packet == NULL) {
        Serial.println("Failed to build join request packet");
        return false;
    }

    size_t packet_len = 6 + 1 + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }

    free(packet);

    Serial.println("Sending hex: " + hexStr);
    Serial2.print("radio tx " + hexStr);
    Serial2.print("\r\n");
    delay(500);
    String response = "";
    while (Serial2.available()) {
        response += (char)Serial2.read();
    }
    Serial.println("TX response: [" + response + "]");
    return response.indexOf("ok") >= 0;
}

bool receiveJoinAccept(NodeConfig& config) {
    Serial.println("Waiting for join accept...");
    Serial2.print("radio rx 0\r\n");
    while (Serial2.available()) Serial2.readStringUntil('\n');
    unsigned long start = millis();
    const unsigned long timeout = 5000;

    while (millis() - start < timeout) {
        yield();

        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');
            Serial.println("RN2483: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                int startIdx = response.indexOf("radio_rx") + 9;
                String hexPayload = response.substring(startIdx);
                hexPayload.trim();
                Serial.println("Received hex within JoinAccept: " + hexPayload);

                if (hexPayload.length() >= 8) {
                    String packetType = hexPayload.substring(4, 6);
                    Serial.println("Packet type: " + packetType);

                    if (packetType == "12") {
                        config.nodeId = strtol(hexPayload.substring(10, 12).c_str(), NULL, 16);
                        config.tdmaSlot = strtol(hexPayload.substring(12, 14).c_str(), NULL, 16);
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