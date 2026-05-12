#include <Arduino.h>
#include "config.h"
#include "join.h"
#include "packetBuilder.h"
#include "lora.h"
#include "security.h"

/*
 * Packet type byte allocation:
 * 0x0F — JOIN request        (light node → gateway)
 * 0x11 — Beacon              (gateway → all nodes)
 * 0x12 — JOIN accept         (gateway → light node)
 * 0x01 — Sensor data uplink  (sensor node → gateway)
 * 0x02 — Light command       (gateway → light node): 0x00 = off, 0x01 = on
 * 0x03 — Light state report  (light node → gateway): 0x00 = off, 0x01 = on
 */

constexpr int LED_PIN = 2;

uint8_t assignedNodeId = 0;
unsigned long assignedSleepSeconds = 0;

bool ledState = false;

void setLed(bool on) {
    ledState = on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    Serial.printf("LED %s\n", on ? "ON" : "OFF");
}

bool sendStateReport() {
    unsigned char dst = 0x00;
    unsigned char payload = ledState ? 0x01 : 0x00;

    unsigned char* packet = build_packet(0x03, &payload, &dst, 1);
    if (packet == NULL) {
        Serial.println("Failed to build state report packet");
        return false;
    }

    size_t packet_len = 5 + 1 + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }
    free(packet);

    Serial2.print("radio rxstop\r\n");
    delay(200);
    Serial2.readStringUntil('\n');
    Serial2.readStringUntil('\n');

    Serial.println("Sending state report: " + hexStr);
    Serial2.print("radio tx " + hexStr + "\r\n");
    delay(500);

    String response = "";
    while (Serial2.available()) {
        response += (char)Serial2.read();
    }
    Serial.println("TX response: [" + response + "]");

    // Go back to receive mode
    Serial2.print("radio rx 0\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    return response.indexOf("ok") >= 0;
}

void handleCommand(String hexPayload) {
    Serial.println("handleCommand called with: " + hexPayload);
    
    // Convert hex to raw bytes for decryption
    uint8_t rawBytes[11];
    for (int i = 0; i < 11; i++) {
        rawBytes[i] = strtol(hexPayload.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
    }
    
    unsigned char plaintext[1];
    if (decrypt_packet(rawBytes, 11, plaintext) != 0) {
        Serial.println("Command MIC verification failed — ignoring");
        return;
    }
    
    Serial.printf("Decrypted command byte: 0x%02X\n", plaintext[0]);

    if (plaintext[0] == 0x01) {
        setLed(true);
    } else if (plaintext[0] == 0x00) {
        setLed(false);
    } else {
        Serial.println("Unknown command — ignoring");
        return;
    }

    sendStateReport();
}
void listenForCommands() {
    Serial.println("Listening for commands...");
    Serial2.print("radio rxstop\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');
    
    Serial2.print("radio rx 0\r\n");
    delay(500); // increase delay
    String rxResponse = Serial2.readStringUntil('\n');
    Serial.println("RX mode response: " + rxResponse);
    while (Serial2.available()) Serial2.readStringUntil('\n');


    while (true) {
        yield();
        delay(50);

        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');

            if (response == "ok" || response == "ok\r") continue;

            Serial.println("Radio received: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                int startIdx = response.indexOf("radio_rx") + 9;
                String hexPayload = response.substring(startIdx);
                hexPayload.trim();

                Serial.println("Hex payload: " + hexPayload);
                Serial.println("Packet type at 4-5: " + hexPayload.substring(4, 6));

                String packetType = hexPayload.substring(4, 6);
                if (packetType == "02") {
                    Serial.println("Command packet detected");
                    handleCommand(hexPayload);
                } else {
                    Serial.println("Not a command packet, type: " + packetType + " — ignoring");
                    Serial2.print("radio rx 0\r\n");
                    delay(200);
                    while (Serial2.available()) Serial2.readStringUntil('\n');
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    initLoRa();

    assignedNodeId = 5; // hardcoded light node ID — agree this with gateway
    source_address = &assignedNodeId;
    Serial.printf("Light node ready — Node ID: %d\n", assignedNodeId);

    listenForCommands();
}

void loop() {}