#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "security.h"

unsigned char* source_address = NULL;
static uint16_t packet_counter = 0;
unsigned char CRC8(const unsigned char* data, size_t len);


unsigned char* build_header(unsigned char type, const unsigned char* des) {
    unsigned char* header = (unsigned char*)malloc(5);
    if (header == NULL) return NULL;

    header[0] = (source_address != NULL) ? *source_address : 0x00;
    header[1] = (des != NULL) ? *des : 0x00;
    header[2] = type;
    header[3] = (packet_counter >> 8) & 0xFF;
    header[4] = packet_counter & 0xFF;

    return header;
}

unsigned char* build_packet(unsigned char type, const unsigned char* data, const unsigned char* des, size_t data_len) {
    if (data == NULL) return NULL;

    unsigned char* header = build_header(type, des);
    if (header == NULL) return NULL;

    const size_t header_len = 5;

    // Encrypt payload and generate MIC
    unsigned char* enc_out = (unsigned char*)malloc(data_len);
    unsigned char mic_out[4];
    if (enc_out == NULL) {
        free(header);
        return NULL;
    }

    int ret = encrypt_and_mic(header, data, data_len, enc_out, mic_out);
    if (ret != 0) {
        free(header);
        free(enc_out);
        return NULL;
    }

    // Assemble: header(5) + encrypted payload + MIC(4) + CRC(1)
    size_t packet_size = header_len + data_len + 4 + 1;
    unsigned char* packet = (unsigned char*)malloc(packet_size);
    if (packet == NULL) {
        free(header);
        free(enc_out);
        return NULL;
    }

    memset(packet, 0, packet_size);
    memcpy(packet, header, header_len);
    memcpy(packet + header_len, enc_out, data_len);
    memcpy(packet + header_len + data_len, mic_out, 4);
    packet[packet_size - 1] = CRC8(enc_out, data_len);

    packet_counter++;
    free(header);
    free(enc_out);

    return packet;
}

unsigned char* MIC(unsigned char* type, const unsigned char* des) {
    (void)type;
    (void)des;
    return NULL;
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

void set_packet_counter(uint16_t value) {
    packet_counter = value;
}

uint16_t get_packet_counter() {
    return packet_counter;
}