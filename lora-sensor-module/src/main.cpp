#include <Arduino.h>
#include "config.h"
#include "payload.h"

// Declared in sensors.cpp
void initSensors();
SensorData readSensors();

// Declared in lora.cpp
void initLoRa();
bool sendPayload(const uint8_t* payload, size_t length);

// Declared in power.cpp
void goToSleep();


void setup() {
    Serial.begin(115200);
    delay(1000);

    initSensors();
    // initLoRa();

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

    // bool ok = sendPayload(payload, length);
    // Serial.print("Send status: ");
    // Serial.println(ok ? "OK" : "FAIL");

    // goToSleep(); // comment this out too, or you won't see anything
}


void loop() {} //node never loops. It starts, reads, sends data and sleeps.
