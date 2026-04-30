#include <Arduino.h>
#include "config.h"
#include "payload.h"
#include "join.h"
#include "packetBuilder.h"

// Runtime config assigned during join
uint8_t assignedNodeId = 0;
unsigned long assignedSleepSeconds = 600; // fallback default

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

    // Join network and get config
    NodeConfig config;
    if (joinNetwork(config)) {
        assignedNodeId = config.nodeId;
        assignedSleepSeconds = config.sleepMs / 1000;
        if (assignedSleepSeconds == 0) assignedSleepSeconds = 10;
        source_address = &assignedNodeId;
        Serial.printf("Node ID: %d, Sleep: %lu s, Slot: %d\n",
            config.nodeId, assignedSleepSeconds, config.tdmaSlot);
    } else {
        Serial.println("Join failed — using defaults");
    }

    SensorData data = readSensors();

    Serial.print("Temperature: ");
    Serial.println(data.temperatureC);
    Serial.print("Humidity: ");
    Serial.println(data.humidity);
    Serial.print("Light: ");
    Serial.println(data.light);

    uint8_t payload[16];
    size_t length = 0;
    buildPayload(data, payload, length);

    Serial.print("Payload bytes: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", payload[i]);
    }
    Serial.println();

    bool ok = sendPayload(payload, length);
    Serial.print("Send status: ");
    Serial.println(ok ? "OK" : "FAIL");

    goToSleep(assignedSleepSeconds);
}

void loop() {}