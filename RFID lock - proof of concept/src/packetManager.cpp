#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
extern unsigned char* source_address;
 
unsigned char* build_header(unsigned char type, const unsigned char* des, uint16_t counter) {
    unsigned char* header = (unsigned char*)malloc(5);
    if (header == NULL) {
        return NULL;
    }
 
    header[0] = (source_address != NULL) ? *source_address : 0x00;
    header[1] = (des != NULL) ? *des : 0x67;
    header[2] = type;
    header[3] = (counter >> 8) & 0xFF;  // Counter byte one (high byte)
    header[4] = counter & 0xFF;         // Counter byte two (low byte)
 
    return header;
}
 
unsigned char* MIC(unsigned char* type, const unsigned char* des) {
    unsigned char* mic = (unsigned char*)malloc(4);
    if (mic == NULL) {
        return NULL;
    }
    mic[0] = 0xFF;
    mic[1] = 0xFF;
    mic[2] = 0xFF;
    mic[3] = 0xFF;
    return mic;
}
 
unsigned char CRC8(const unsigned char* data, size_t len) {
    unsigned char crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
 
unsigned char* build_packet(unsigned char type, unsigned char* data, size_t data_len, const unsigned char* des, uint16_t counter, size_t* out_packet_len) {
    if (data == NULL) {
        return NULL;
    }
 
    unsigned char* header = build_header(type, des, counter);
    if (header == NULL) {
        return NULL;
    }
 
    const size_t header_len = 5;
    size_t packet_size = header_len + data_len + 4 + 1; // header + data + MIC(4) + CRC(1)
 
    unsigned char* packet = (unsigned char*)malloc(packet_size);
    if (packet == NULL) {
        free(header);
        return NULL;
    }
 
    memcpy(packet, header, header_len);
    memcpy(packet + header_len, data, data_len);
    free(header);
 
    unsigned char* mic = MIC(&type, des);
    if (mic == NULL) {
        free(packet);
        return NULL;
    }
 
    unsigned char crc = CRC8(data, data_len);
 
    memcpy(packet + header_len + data_len, mic, 4);
    packet[header_len + data_len + 4] = crc;
 
    free(mic);
 
    if (out_packet_len != NULL) {
        *out_packet_len = packet_size;
    }
 
    return packet;
}
 
bool verifyPacketMICAndCRC(unsigned char* packet, size_t packetLength) {
    if (packet == NULL || packetLength < 10) { // 5 header + 4 MIC + 1 CRC minimum
        return false;
    }
 
    const size_t header_len = 5;
    size_t dataLength = packetLength - header_len - 4 - 1;
 
    unsigned char* type = &packet[2];
    unsigned char* des  = &packet[1];
    unsigned char* mic  = MIC(type, des);
    unsigned char* data = &packet[header_len];
 
    if (mic == NULL) {
        return false;
    }
 
    unsigned char crc = CRC8(data, dataLength);
 
    bool micMatches = (memcmp(mic, &packet[header_len + dataLength], 4) == 0);
    bool crcMatches = (crc == packet[header_len + dataLength + 4]);
 
    free(mic);
 
    return micMatches && crcMatches;
}
 
unsigned char* buildBeaconPayload(uint8_t SlotDuration, uint32_t cycleTime, uint8_t CurrentSlotCount) {
    unsigned char* payload = (unsigned char*)malloc(6);
    if (payload == NULL) {
        return NULL;
    }
 
    payload[0] = cycleTime & 0xFF;         // Cycle time byte 1 (low byte)
    payload[1] = (cycleTime >> 8)  & 0xFF;
    payload[2] = (cycleTime >> 16) & 0xFF;
    payload[3] = (cycleTime >> 24) & 0xFF;
    payload[4] = SlotDuration;
    payload[5] = CurrentSlotCount;
 
    return payload;
}
 