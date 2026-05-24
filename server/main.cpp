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

uint8_t lightAddress = 0x05; 

unsigned char* source_address = NULL;

uint32_t cycleTime; // in ms
unsigned long lastCycleTime; // in ms

uint16_t SlotDuration; // in ms
uint8_t CurrentSlotCount; // in ms

uint16_t counter = 0x0001;

unsigned long startTime; // in ms


// NEW: Blynk and WebSocket functions
extern void blynk_setup(const char* ssid, const char* pw);
extern void blynk_loop();
extern void ws_setup();
extern void ws_loop();

// Network task: services WebSocket + Blynk on its own core so the LoRa
// scheduling on the main loop can't starve them. Both libraries are touched
// only from this task (blynk_send() runs inside the WS event handler), so no
// locking is required.
static void network_task(void* /*arg*/) {
    for (;;) {
        ws_loop();
        blynk_loop();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup() {
    source_address = (unsigned char*)malloc(1);
    if (source_address != NULL) {
        *source_address = 0x00;
    }

    cycleTime = 11000; // 11 seconds
    lastCycleTime = 0;
    SlotDuration = 2500; // 2500 ms second
    CurrentSlotCount = 0;
    Serial.begin(115200);

    setupLora();

    // NEW: Connect to WiFi, Blynk and Cibicom WebSocket
    blynk_setup("OnePlus12", "hej23457");
    ws_setup();

    // Run WebSocket + Blynk on core 1 so LoRa cycle timing on core 0 can't
    // block the TLS heartbeat. 16 KiB stack covers TLS + ArduinoJson buffers.
    xTaskCreatePinnedToCore(network_task, "network", 16384, NULL, 1, NULL, 1);
}

void loop() {
    unsigned long currentTime = millis();
    if ((long)(currentTime - lastCycleTime) >= (long)cycleTime) {
        lastCycleTime = currentTime;
        
        unsigned char* data = buildBeaconPayload(SlotDuration/100, cycleTime, CurrentSlotCount);
        if (data == NULL) {
            Serial.println("Failed to build beacon payload");
            return;
        }
        Serial.println("Built beacon payload: " + String(data[0]) + " " + String(data[1]) + " " + String(data[2]) + " " + String(data[3]) + " " + String(data[4]) + " " + String(data[5]));

        size_t packet_len = 0;
        unsigned char* tx_packet = buildPacket(0x11, 0x67, counter, data, 6, &packet_len);
        free(data);
        if (tx_packet == NULL) {
            Serial.println("Failed to build beacon packet");
            return;
        }
        Serial.println("Beacon Packet sent");
        sendMessage(tx_packet, packet_len);
        counter++;
        free(tx_packet);
    }
    delay(100);
    for (int i = 0; i < CurrentSlotCount; i++){
        Serial.println("Listening for slot " + String(i+1) + " of " + String(CurrentSlotCount));
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
        Serial.println("Time used for slot " + String(i+1) + ": " + String((millis() - startTime) / 1000.0) + " s");
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
            unsigned char payload[2];
            payload[0] = slot_value; // destination address is the assigned slot number shifted
            payload[1] = CurrentSlotCount - 1; // 0-based slot index (matches server slot loop i = 0..CurrentSlotCount-1)
            size_t join_packet_len = 0;
            unsigned char* join_packet = buildPacket(0x12, slot_value, counter, payload, 2, &join_packet_len);
            if (join_packet == NULL) {
                Serial.println("Failed to build join response packet");
            } else {
                sendMessage(join_packet, join_packet_len);
                counter++;
                free(join_packet);
                Serial.println("Join response sent");
            }
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