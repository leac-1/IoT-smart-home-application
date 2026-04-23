#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "LoraCom.cpp"
#include "security.h"
#include "packetManager.cpp"
#include "join.cpp"

unsigned char* source_address = NULL;

uint32_t cycleTime; // in ms
int lastCycleTime; // in ms

uint8_t SlotDuration; // in ms
uint8_t CurrentSlotCount; // in ms

uint16_t counter = 0x0001;

void setup() {
    source_address = (unsigned char*)malloc(1);
    if (source_address != NULL) {
        *source_address = 0x00;
    }

    cycleTime = 5000; // 5 seconds
    lastCycleTime = 0;
    SlotDuration = 150; // 1 second
    CurrentSlotCount = 0;
    Serial.begin(115200);

    setupLora();
}

void loop() {
    unsigned long currentTime = millis();
    if ((long)(currentTime - lastCycleTime) >= (long)cycleTime) {
        lastCycleTime = currentTime;
        
        unsigned char* data = buildBeaconPayload(SlotDuration, cycleTime, CurrentSlotCount);
        if (data == NULL) {
            Serial.println("Failed to build beacon payload");
            return;
        }
        size_t data_len = 7; // Length of the beacon payload
        unsigned char des = 0x67;

        unsigned char header[5];
        header[0] = (source_address != NULL) ? *source_address : 0x00;
        header[1] = des;
        header[2] = 0x11;
        header[3] = (counter >> 8) & 0xFF;
        header[4] = counter & 0xFF;

        unsigned char* encrypted_payload = (unsigned char*)malloc(data_len);
        unsigned char mic[4];
        if (encrypted_payload == NULL) {
            Serial.println("Allocation failed for encrypted payload");
            return;
        }

        int enc_ret = encrypt_and_mic(header, data, data_len, encrypted_payload, mic);
        if (enc_ret != 0) {
            Serial.println("encrypt_and_mic failed");
            free(encrypted_payload);
            return;
        }

        size_t packet_len = sizeof(header) + data_len + sizeof(mic) + 1;
        unsigned char* tx_packet = (unsigned char*)malloc(packet_len);
        if (tx_packet == NULL) {
            Serial.println("Allocation failed for tx packet");
            free(encrypted_payload);
            return;
        }

        memcpy(tx_packet, header, sizeof(header));
        memcpy(tx_packet + sizeof(header), encrypted_payload, data_len);
        memcpy(tx_packet + sizeof(header) + data_len, mic, sizeof(mic));
        tx_packet[packet_len - 1] = CRC8(encrypted_payload)[0];
        Serial.println("Packet sent");
        //Serial.print("Encrypted packet length: ");
        //Serial.println(packet_len);
        //Serial.print("Encrypted packet content: ");
        for (size_t i = 0; i < packet_len; i++) {
            //Serial.printf("%02X ", tx_packet[i]);
        }
        //Serial.println();

        unsigned char* decrypted_payload = (unsigned char*)malloc(data_len);
        if (decrypted_payload != NULL) {
            int dec_ret = decrypt_and_verify(
                header,
                encrypted_payload,
                data_len,
                mic,
                decrypted_payload
            );

            if (dec_ret == 0) {
                //Serial.print("Decrypted payload: ");
                for (size_t i = 0; i < data_len; i++) {
                    //Serial.printf("%02X ", decrypted_payload[i]);
                }
                //Serial.println();
            } else {
                Serial.println("decrypt_and_verify failed (MIC mismatch or decryption error)");
            }
            free(decrypted_payload);
        }

        sendMessage(tx_packet, packet_len);
        counter++;

        free(tx_packet);
        free(encrypted_payload);
    }

    unsigned long windowStart = millis();
    unsigned long windowLen = (unsigned long)SlotDuration * (unsigned long)CurrentSlotCount+1;
    
    while (millis() - windowStart < windowLen) {
        if (checkForJoinRequest()) {
            CurrentSlotCount++;
            unsigned char slot_value = (CurrentSlotCount + 5) & 0xFF;
            unsigned char* header = build_header(0x12, &slot_value, counter);
            unsigned char enc_out[2];
            unsigned char mic_out[4];
            unsigned char* payload = (unsigned char*)malloc(2); // Empty payload for join response
            payload[0] = slot_value; // destination address is the assigned slot number shifted
            payload[1] = CurrentSlotCount; // Inform the device of its assigned slot

            encrypt_and_mic(header, payload, 2, enc_out, mic_out);
            size_t join_packet_len = sizeof(header) + sizeof(enc_out) + sizeof(mic_out) + 1;
            unsigned char* join_packet = (unsigned char*)malloc(join_packet_len);
            memcpy(join_packet, header, sizeof(header));
            memcpy(join_packet + sizeof(header), enc_out, sizeof(enc_out));
            memcpy(join_packet + sizeof(header) + sizeof(enc_out), mic_out, sizeof(mic_out));
            join_packet[join_packet_len - 1] = CRC8(enc_out)[0];
            sendMessage(join_packet, join_packet_len);
            free(header);
            free(payload);
            free(join_packet);
            Serial.println("Join response sent");
        }
        delay(25);
    }
    delay(25);
}

extern "C" void app_main(void) {
    initArduino();

    setup();

    while (true) {
        loop();
    }
}
