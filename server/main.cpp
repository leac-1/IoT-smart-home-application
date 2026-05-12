#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "LoraCom.cpp"
#include "security.h"
#include "packetManager.cpp"
#include "join.cpp"
#include "LoraCom.h"

// NEW: Blynk and WebSocket functions
extern void blynk_setup(const char* ssid, const char* pw);
extern void blynk_loop();
extern void ws_setup();
extern void ws_loop();

unsigned char* source_address = NULL;

uint32_t cycleTime; // in ms
unsigned long lastCycleTime; // in ms

uint8_t SlotDuration; // in ms
uint8_t CurrentSlotCount; // in ms

uint16_t counter = 0x0001;

unsigned long startTime; // in ms


void setup() {
    source_address = (unsigned char*)malloc(1);
    if (source_address != NULL) {
        *source_address = 0x00;
    }

    cycleTime = 11000; // 11 seconds
    lastCycleTime = 0;
    SlotDuration = 250; // 250 ms second
    CurrentSlotCount = 0;
    Serial.begin(115200);

    setupLora();

    // NEW: Connect to WiFi, Blynk and Cibicom WebSocket
    blynk_setup("OnePlus12", "hej23457");
    ws_setup();
}

void loop() {
    unsigned long currentTime = millis();
    if ((long)(currentTime - lastCycleTime) >= (long)cycleTime) {
        lastCycleTime = currentTime;
        
        unsigned char* data = buildBeaconPayload(SlotDuration, cycleTime, CurrentSlotCount);
        if (data == NULL) {
            Serial.println("Failed to build beacon payload");
            return;
        }
        size_t data_len = 6; // Length of the beacon payload
        unsigned char des = 0x67;

        unsigned char header[5];
        header[0] = (source_address != NULL) ? *source_address : 0x00;
        header[1] = des;
        header[2] = 0x11;
        header[3] = (counter >> 8) & 0xFF;
        header[4] = counter & 0xFF;

        unsigned char* encrypted_payload = (unsigned char*)malloc(data_len);
        unsigned char mic[4];
        if (encrypted_payload == NULL) {
            Serial.println("Allocation failed for encrypted payload");
            return;
        }

        int enc_ret = encrypt_and_mic(header, data, data_len, encrypted_payload, mic);
        if (enc_ret != 0) {
            Serial.println("encrypt_and_mic failed");
            free(encrypted_payload);
            return;
        }

        size_t packet_len = sizeof(header) + data_len + sizeof(mic) + 1;
        unsigned char* tx_packet = (unsigned char*)malloc(packet_len);
        if (tx_packet == NULL) {
            Serial.println("Allocation failed for tx packet");
            free(encrypted_payload);
            return;
        }

        memcpy(tx_packet, header, sizeof(header));
        memcpy(tx_packet + sizeof(header), encrypted_payload, data_len);
        memcpy(tx_packet + sizeof(header) + data_len, mic, sizeof(mic));
        tx_packet[packet_len - 1] = CRC8(encrypted_payload, data_len);
        Serial.println("Beacon Packet sent");

        sendMessage(tx_packet, packet_len);
        counter++;
        free(tx_packet);
        free(encrypted_payload);
    }

    for (int i = 0; i < CurrentSlotCount; i++){
        startTime = millis();
        while ((millis() - lastCycleTime) < (unsigned long)(i+1) * SlotDuration) {
            if (packetRecieved()) {
                Serial.println("Data packet received");
                size_t packet_len = 0;
                unsigned char* packet = getReceivedPacket(&packet_len);
                if (packet != NULL) {
                    handleDataPacket(packet, packet_len);
                    free(packet);
                } else {
                    Serial.println("Failed to retrieve received packet");
                }
            }

            delay(25);
        }
        Serial.println("Time used for slot: " + String((millis() - startTime) / 1000.0) + " s");
    }

    unsigned long windowStart = millis();
    unsigned long elapsed = windowStart - lastCycleTime;
    unsigned long windowLen = (elapsed < cycleTime) ? (cycleTime - elapsed) : 0;
    startTime = millis();
    while (millis() - windowStart < windowLen) {
        if (checkForJoinRequest()) {
            Serial.println("Join request received");
            delay(1500);
            CurrentSlotCount++;
            CurrentSlotCount++;
            unsigned char slot_value = (CurrentSlotCount + 5) & 0xFF;
            const size_t header_len = 5;
            unsigned char* header = build_header(0x12, &slot_value, counter);
            unsigned char enc_out[2];
            unsigned char mic_out[4];
            unsigned char* payload = (unsigned char*)malloc(2);
            payload[0] = slot_value;
            payload[1] = CurrentSlotCount - 1;
            encrypt_and_mic(header, payload, 2, enc_out, mic_out);
            size_t join_packet_len = header_len + sizeof(enc_out) + sizeof(mic_out) + 1;
            unsigned char* join_packet = (unsigned char*)malloc(join_packet_len);
            memcpy(join_packet, header, header_len);
            memcpy(join_packet + header_len, enc_out, sizeof(enc_out));
            memcpy(join_packet + header_len + sizeof(enc_out), mic_out, sizeof(mic_out));
            join_packet[join_packet_len - 1] = CRC8(enc_out, sizeof(enc_out));
            sendMessage(join_packet, join_packet_len);
            counter++;
            free(header);
            free(payload);
            free(join_packet);
            Serial.println("Join response sent");
        } else if (loraSerial.available()) {
            Serial.println("Data packet received (in join window)");
            size_t packet_len = 0;
            unsigned char* packet = getReceivedPacket(&packet_len);
            if (packet != NULL) {
                handleDataPacket(packet, packet_len);
                free(packet);
            } else {
                Serial.println("Failed to retrieve received packet");
            }
        }
        delay(25);
    }

    // NEW: Keep Blynk and WebSocket alive
    blynk_loop();
    ws_loop();

    Serial.println("Empty time used in cycle: " + String((millis() - startTime) / 1000.0) + " s");
    Serial.println("Cycle complete. Total slots: " + String(CurrentSlotCount));
    Serial.println("-------------------------------");
}

extern "C" void app_main(void) {
    initArduino();

    setup();

    while (true) {
        loop();
    }
}