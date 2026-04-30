#include <Arduino.h>
#include "config.h"
#include "payload.h"
#include "join.h"
#include "packetBuilder.h"

RTC_DATA_ATTR uint8_t rtcNodeId = 0;
RTC_DATA_ATTR uint8_t rtcTdmaSlot = 0;
RTC_DATA_ATTR bool rtcJoined = false;

// Runtime config assigned during join
uint8_t assignedNodeId = 0;
unsigned long assignedSleepSeconds = 10; // fallback default

// Declared in sensors.cpp
void initSensors();
SensorData readSensors();

// Declared in lora.cpp
void initLoRa();
bool sendPayload(const uint8_t* payload, size_t length);

// Declared in power.cpp
void goToSleep(unsigned long seconds);

void setup() {
    Serial.begin(115200);
    delay(1000);
    initSensors();
    initLoRa();

    if (!rtcJoined) {
        NodeConfig config;
        if (joinNetwork(config)) {
            rtcNodeId = config.nodeId;
            rtcTdmaSlot = config.tdmaSlot;
            rtcJoined = true;
            assignedNodeId = rtcNodeId;
            source_address = &assignedNodeId;
        } else {
            Serial.println("Join failed — using defaults");
            goToSleep(10);
            return;
        }
    } else {
        assignedNodeId = rtcNodeId;
        source_address = &assignedNodeId;
        Serial.printf("Resuming — Node ID: %d, Slot: %d\n", rtcNodeId, rtcTdmaSlot);
    }
}

void loop() {
    NodeConfig config;
    config.tdmaSlot = rtcTdmaSlot;

    if (!waitForBeaconWithTDMA(config)) {
        Serial.println("Failed to get TDMA beacon");
        delay(1000);
        return;
    }

    unsigned long sleepUntilSlot = 0;
    if (config.tdmaSlot > config.currentSlotCount) {
        sleepUntilSlot = (config.tdmaSlot - config.currentSlotCount) * config.slotDuration;
    } else if (config.tdmaSlot < config.currentSlotCount) {
        sleepUntilSlot = (config.cycleTime - config.currentSlotCount * config.slotDuration)
                       + config.tdmaSlot * config.slotDuration;
    }

    if (sleepUntilSlot > 0) delay(sleepUntilSlot);

    SensorData data = readSensors();
    Serial.printf("Temperature: %.2f, Humidity: %.2f, Light: %d\n",
        data.temperatureC, data.humidity, data.light);

    uint8_t payload[16];
    size_t length = 0;
    buildPayload(data, payload, length);

    Serial.print("Payload bytes: ");
    for (size_t i = 0; i < length; i++) Serial.printf("%02X ", payload[i]);
    Serial.println();

    bool ok = sendPayload(payload, length);
    Serial.println(ok ? "Send status: OK" : "Send status: FAIL");

    unsigned long timeUsed = sleepUntilSlot + config.slotDuration;
    unsigned long remainingCycle = 0;
    if (config.cycleTime > timeUsed) {
        remainingCycle = config.cycleTime - timeUsed;
    }

    Serial.printf("Remaining cycle: %lu ms\n", remainingCycle);

    if (remainingCycle > 10000) {
        goToSleep(remainingCycle / 1000);
    } else {
        delay(remainingCycle);
    }
}