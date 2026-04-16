#include <stdlib.h>
#include <string.h>
#include <stddef.h>

unsigned char* source_address = NULL; // was extern, now defined here

unsigned char* build_header(unsigned char type, const unsigned char* des) {
    unsigned char* header = (unsigned char*)malloc(6);
    if (header == NULL) {
        return NULL;
    }

    header[0] = 0x07;
    header[1] = (source_address != NULL) ? *source_address : 0x00;
    header[2] = (des != NULL) ? *des : 0x00;
    header[3] = type;
    header[4] = 0x00; // Counter byte one
    header[5] = 0x00; // Counter byte two

    return header;
}

unsigned char* build_packet(unsigned char type, const unsigned char* data, const unsigned char* des, size_t data_len) {
    if (data == NULL) {
        return NULL;
    }

    unsigned char* header = build_header(type, des);
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

    return packet;
}

unsigned char* MIC(unsigned char* type, const unsigned char* des) {
    (void)type;
    (void)des;
    return NULL;
}

unsigned char* CRC8(char* data) {
    return NULL;
}