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
    return data;
}