#include "payload.h"

void buildPayload(const SensorData& data, uint8_t* buffer, size_t& length) {
    // Scale floats to integers to avoid sending 4 bytes per value
    int16_t  temp  = (int16_t)(data.temperatureC * 10); // e.g. 23.4°C → 234
    uint16_t hum   = (uint16_t)(data.humidity * 10);    // e.g. 61.2% → 612
    uint16_t light = (uint16_t)data.light;              // 0–4095

    buffer[0] = (temp  >> 8) & 0xFF; // extract high byte
    buffer[1] =  temp        & 0xFF; // extract low byte
    buffer[2] = (hum   >> 8) & 0xFF;
    buffer[3] =  hum         & 0xFF;
    buffer[4] = (light >> 8) & 0xFF;
    buffer[5] =  light       & 0xFF;

    length = 6;
}

