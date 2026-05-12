#include <Arduino.h>
#include <DHT.h>
#include "pins.h"
#include "payload.h"

static DHT dht(DHT_PIN, DHT11);

void initSensors() {
    dht.begin();
}

SensorData readSensors() {
    SensorData data{};
    data.temperatureC = dht.readTemperature();
    data.humidity = dht.readHumidity();
    data.light = analogRead(LDR_PIN);
    int batteryRaw = analogRead(BATTERY_PIN);
    data.batteryLow = (batteryRaw < 820); // below 20% (0.66V)
    return data;
}