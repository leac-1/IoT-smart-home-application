#include <Arduino.h>
#include "pins.h"
#include "secrets.h"
#include "packetBuilder.h"
#include "config.h"

// RN2483 communicates over UART via ASCII commands
// Serial2 uses GPIO18 (TX) and GPIO19 (RX) on the ESP32

void initLoRa() {
    Serial2.begin(57600, SERIAL_8N1, RN2483_RX_PIN, RN2483_TX_PIN);

    // Hard reset
    pinMode(RN2483_RST_PIN, OUTPUT);
    digitalWrite(RN2483_RST_PIN, LOW);
    delay(10);
    digitalWrite(RN2483_RST_PIN, HIGH);
    delay(500);

    static unsigned char nodeId = NODE_ID;
    source_address = &nodeId;
}


bool sendPayload(const uint8_t* payload, size_t length) {
    unsigned char dst = 0x00; // Gateway
    
    unsigned char* packet = build_packet(0x01, payload, &dst, length);
    if (packet == NULL) {
        Serial.println("Failed to build packet");
        return false;
    }

    // Convert packet to hex string for RN2483 transmission
    size_t packet_len = 6 + length + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }

    free(packet);

    // Send packet using RN2483
    Serial2.println("radio tx " + hexStr);

    // Wait for response
    String response = Serial2.readStringUntil('\n');
    Serial.println("RN2483: " + response);

    return response.indexOf("ok") >= 0;
}