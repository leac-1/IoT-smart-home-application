#include <Arduino.h>
#include "LoraCom.h"
#include "security.h"

void handleDataPacket(unsigned char* packet, size_t packet_len);

bool checkForJoinRequest() {
    if (!loraSerial.available()) return false;

    String request = loraSerial.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(request);
    if (!request.startsWith("radio_rx")) return false;

    int startIdx = request.indexOf("radio_rx") + 10;
    if (startIdx < 10 || startIdx >= (int)request.length()) return false;
    String hexPayload = request.substring(startIdx);
    hexPayload.trim();

    size_t totalBytes = hexPayload.length() / 2;
    if (totalBytes < 5) return false;

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    auto hexByte = [&](size_t i) -> unsigned char {
        return (unsigned char)((nibble(hexPayload[2 * i]) << 4) | nibble(hexPayload[2 * i + 1]));
    };

    unsigned char h0 = hexByte(0), h1 = hexByte(1), h2 = hexByte(2);

    // Join request layout: 5 header + 1 encrypted payload + 4 MIC + 1 CRC = 11 bytes (10 without CRC).
    // src=0x00 (unjoined), dst=0x00 (gateway), type=0x0F (JOIN).
    bool is_join_shape = (h0 == 0x00 && h1 == 0x00 && h2 == 0x0F)
                         && (totalBytes == 10 || totalBytes == 11);

    if (!is_join_shape) {
        // Data packet drifted into the join window — route it to the data handler
        // instead of dropping it.
        unsigned char* packet = (unsigned char*)malloc(totalBytes);
        if (packet == NULL) return false;
        for (size_t i = 0; i < totalBytes; i++) packet[i] = hexByte(i);
        Serial.println("Data packet received (in join window)");
        handleDataPacket(packet, totalBytes);
        free(packet);
        return false;
    }

    unsigned char header[5];
    unsigned char ciphertext[1];
    unsigned char mic[4];
    unsigned char decrypted_payload[1];

    for (size_t i = 0; i < 5; i++) header[i] = hexByte(i);
    ciphertext[0] = hexByte(5);
    for (size_t i = 0; i < 4; i++) mic[i] = hexByte(6 + i);

    int ret = decrypt_and_verify(header, ciphertext, 1, mic, decrypted_payload);
    if (ret == 0) {
        Serial.println("Join request detected");
        return true;
    }
    Serial.println("decrypt_and_verify failed (MIC mismatch or decryption error)");
    return false;
}