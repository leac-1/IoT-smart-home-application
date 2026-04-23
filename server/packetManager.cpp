#include <stdlib.h>
#include <string.h>
#include <stddef.h>
extern unsigned char* source_address;

unsigned char* build_header(unsigned char type, const unsigned char* des, unsigned char counter) {
    unsigned char* header = (unsigned char*)malloc(6);
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

    mic[0] = 0xFF;
    mic[1] = 0xFF;
    mic[2] = 0xFF;
    mic[3] = 0xFF;
    return mic;
}

unsigned char* CRC8(unsigned char* data) {

    unsigned char* crc = (unsigned char*)malloc(1);
    if (crc == NULL) {
        return NULL;
    }
    crc[0] = 0x00;

    for (size_t i = 0; i < 4; i++) {
        crc[0] ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc[0] & 0x80) {
                crc[0] = (crc[0] << 1) ^ 0x07;
            } else {
                crc[0] <<= 1;
            }
        }
    }

    return crc;
}

unsigned char* build_packet(unsigned char type, unsigned char* data, size_t data_len, const unsigned char* des, unsigned char counter, size_t* out_packet_len) {
    if (data == NULL) {
        return NULL;
    }

    unsigned char* header = build_header(type, des, counter);
    if (header == NULL) {
        return NULL;
    }

    const size_t header_len = 6;
    size_t packet_size = header_len + data_len + 4 + 1;

    unsigned char* packet = (unsigned char*)malloc(packet_size);
    if (packet == NULL) {
        free(header);
        return NULL;
    }

    memcpy(packet, header, header_len);
    memcpy(packet + header_len, data, data_len);
    free(header);

    unsigned char* mic = MIC(&type, des);
    unsigned char* crc = CRC8(data);
    if (mic == NULL || crc == NULL) {
        free(mic);
        free(crc);
        free(packet);
        return NULL;
    }

    memcpy(packet + header_len + data_len, mic, 4);
    memcpy(packet + header_len + data_len + 4, crc, 1);

    free(mic);
    free(crc);

    if (out_packet_len != NULL) {
        *out_packet_len = packet_size;
    }

    return packet;
}

bool verifyPacketMICAndCRC(unsigned char* packet, size_t packetLength) {
    if (packet == NULL || packetLength < 11) {
        return false;
    }

    unsigned char* type = &packet[3];
    unsigned char* des = &packet[2];
    unsigned char* mic = MIC(type, des);
    unsigned char* data = &packet[6];
    unsigned char* crc = CRC8(data);

    size_t dataLength = packetLength - 6 - 4 - 1;

    if (mic == NULL || crc == NULL) {
        free(mic);
        free(crc);
        return false;
    }

    bool micMatches = (memcmp(mic, &packet[6 + dataLength], 4) == 0);
    bool crcMatches = (memcmp(crc, &packet[6 + dataLength + 4], 1) == 0);

    free(mic);
    free(crc);

    return micMatches && crcMatches;
}


unsigned char* buildBeaconPayload(uint8_t SlotDuration, uint32_t cycleTime, uint8_t CurrentSlotCount) {

    unsigned char* payload = (unsigned char*)malloc(7);
    if (payload == NULL) {
        return NULL;
    }

    payload[0] = cycleTime & 0xFF; // Cycle time byte 1 (low byte)
    payload[1] = (cycleTime >> 8) & 0xFF;
    payload[2] = (cycleTime >> 16) & 0xFF;
    payload[3] = (cycleTime >> 24) & 0xFF;
    payload[4] = SlotDuration;
    payload[5] = CurrentSlotCount;

    return payload;
}


