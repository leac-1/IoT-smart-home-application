#include "payload.h"

void buildPayload(const SensorData& data, uint8_t* buffer, size_t& length) {
    int16_t  temp  = (int16_t)(data.temperatureC * 10);
    uint16_t hum   = (uint16_t)(data.humidity * 10);
    uint16_t light = (uint16_t)data.light;

    buffer[0] = (temp  >> 8) & 0xFF;
    buffer[1] =  temp        & 0xFF;
    buffer[2] = (hum   >> 8) & 0xFF;
    buffer[3] =  hum         & 0xFF;
    buffer[4] = (light >> 8) & 0xFF;
    buffer[5] =  light       & 0xFF;

    length = 6;
}