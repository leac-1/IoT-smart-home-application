#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "security.h"
#include "LoraCom.h"
extern unsigned char* source_address;

unsigned char* build_header(unsigned char type, const unsigned char* des, uint16_t counter) {
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

bool verifyPacketCRC(unsigned char* packet, unsigned char* crc) {
    
    unsigned char computed_crc = CRC8(packet, strlen((char*)packet));

    bool is_valid = (computed_crc == crc[0]);
    return is_valid;
}


unsigned char* buildBeaconPayload(uint8_t SlotDuration, uint32_t cycleTime, uint8_t CurrentSlotCount) {

    unsigned char* payload = (unsigned char*)malloc(6);
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

void handleDataPacket(unsigned char* packet, size_t packet_len) {
    const size_t header_len = 5;
    const size_t mic_len = 4;
    const size_t crc_len = 1;
    if (packet_len < header_len + mic_len + crc_len) {
        Serial.println("Packet too short");
        return;
    }
    size_t data_len = packet_len - header_len - mic_len - crc_len;

    unsigned char* header = packet;
    unsigned char* data = packet + header_len;
    unsigned char* mic = packet + header_len + data_len;
    unsigned char* recieved_crc = packet + header_len + data_len + mic_len;

    if (!verifyPacketCRC(data, recieved_crc)) {
        Serial.println("CRC check failed");
        return;
    }

    unsigned char* decrypted = (unsigned char*)malloc(data_len);
    if (decrypted == NULL) {
        Serial.println("Allocation failed for decrypted payload");
        return;
    }

    int ret = decrypt_and_verify(header, data, data_len, mic, decrypted);
    if (ret == 0) {
        Serial.print("Decrypted payload: ");
        for (size_t i = 0; i < data_len; i++) {
            Serial.printf("%02X ", decrypted[i]);
        }
        Serial.println();
    } else {
        Serial.println("decrypt_and_verify failed (MIC mismatch or decryption error)");
    }
    free(decrypted);
}

bool packetRecieved() {
    if (loraSerial.available()) {
        return true;
    } else {
        return false;
    }
}

unsigned char* getReceivedPacket(size_t* out_len) {
    if (out_len != NULL) *out_len = 0;

    String request = loraSerial.readStringUntil('\n');
    if (request.startsWith("radio_err")) {
        rearmReceive();
        return NULL;
    }
    if (!request.startsWith("radio_rx")) {
        return NULL;
    }

    int startIdx = request.indexOf("radio_rx") + 10;
    if (startIdx < 10 || startIdx >= (int)request.length()) {
        return NULL;
    }
    String hexPayload = request.substring(startIdx);
    hexPayload.trim();

    size_t totalBytes = hexPayload.length() / 2;
    if (totalBytes == 0) {
        return NULL;
    }

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    unsigned char* packet = (unsigned char*)malloc(totalBytes);
    if (packet == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < totalBytes; i++) {
        packet[i] = (unsigned char)((nibble(hexPayload[2 * i]) << 4) | nibble(hexPayload[2 * i + 1]));
    }

    if (out_len != NULL) *out_len = totalBytes;
    return packet;
}
