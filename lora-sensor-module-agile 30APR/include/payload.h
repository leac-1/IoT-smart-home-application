#pragma once
#include <stdint.h>
#include <stddef.h>

struct SensorData {
    float temperatureC;
    float humidity;
    int light;
};

void buildPayload(const SensorData& data, uint8_t* buffer, size_t& length);