#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "security.h"
#include "LoraCom.h"
extern unsigned char* source_address;
String door_state = "unknown"; // "open", "closed" or "unknown"

// Curtain control constants
#define CURTAIN_NODE_ADDR 0x06
#define HOT_THRESHOLD_C 26.0
#define COLD_THRESHOLD_C 20.0
#define LIGHT_BRIGHT_THRESHOLD 2500

// Track last known curtain state on server (0=unknown,1=open,2=closed)
static uint8_t lastCurtainState = 0;

// External symbols from main/LoraCom
extern uint16_t counter;
void sendMessage(unsigned char* packet, size_t packet_len);

int temp0Pin = 0; // V pin double
int lightLevelPin = 1; // V pin int
int curtainStatePin = 2; // V pin String
int lampStatePin = 3; // V pin String 
int doorPin = 4; // V pin String
int humidityPin = 5; // V pin int
int batteryPin = 6; // V pin String
int temp1Pin = 7; // V pin double
int LoRaWANtempPin = 8; // V pin double

extern void blynk_send(int pin, double value);
extern void blynk_send(int pin, const char* value);

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

bool verifyPacketCRC(const unsigned char* data, size_t len, unsigned char crc) {
    return CRC8(data, len) == crc;
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

// Build a full LoRa packet: [header(5) | encrypted_payload(payload_len) | MIC(4) | CRC(1)].
// Caller owns the returned buffer and must free() it. Returns NULL on failure
// (with *out_len set to 0). Does NOT send the packet or touch the counter —
// the caller is responsible for sendMessage() and counter++.
unsigned char* buildPacket(unsigned char type, unsigned char dest, uint16_t counter,
                           const unsigned char* payload, size_t payload_len,
                           size_t* out_len) {
    if (out_len != NULL) *out_len = 0;
    if (payload == NULL || payload_len == 0) return NULL;

    const size_t header_len = 5;
    const size_t mic_len = 4;
    const size_t crc_len = 1;

    unsigned char* header = build_header(type, &dest, counter);
    if (header == NULL) return NULL;

    unsigned char* enc_out = (unsigned char*)malloc(payload_len);
    if (enc_out == NULL) {
        free(header);
        return NULL;
    }
    unsigned char mic_out[4];

    if (encrypt_and_mic(header, payload, payload_len, enc_out, mic_out) != 0) {
        free(header);
        free(enc_out);
        return NULL;
    }

    size_t packet_len = header_len + payload_len + mic_len + crc_len;
    unsigned char* packet = (unsigned char*)malloc(packet_len);
    if (packet == NULL) {
        free(header);
        free(enc_out);
        return NULL;
    }

    memcpy(packet, header, header_len);
    memcpy(packet + header_len, enc_out, payload_len);
    memcpy(packet + header_len + payload_len, mic_out, mic_len);
    packet[packet_len - 1] = CRC8(enc_out, payload_len);

    free(header);
    free(enc_out);

    if (out_len != NULL) *out_len = packet_len;
    return packet;
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

    if (!verifyPacketCRC(data, data_len, *recieved_crc)) {
        Serial.println("CRC check failed");
        Serial.print("Received packet: ");
        for (size_t i = 0; i < packet_len; i++) {
            Serial.printf("%02X ", packet[i]);
        }
        Serial.println();
        return;
    }

    unsigned char* decrypted = (unsigned char*)malloc(data_len);
    if (decrypted == NULL) {
        Serial.println("Allocation failed for decrypted payload");
        return;
    }

    int ret = decrypt_and_verify(header, data, data_len, mic, decrypted);
    if (ret == 0) {
        Serial.println("Packet decrypted and verified successfully");
        Serial.print("Decrypted payload: ");
        for (size_t i = 0; i < data_len; i++) {
            Serial.printf("%02X ", decrypted[i]);
        }
        Serial.println();

        switch (header[2]) {
            case 0x01: {
                Serial.println("Handling data packet...");
                // Temperature is signed int16 in tenths of degrees
                int16_t temp10 = ((int16_t)decrypted[0] << 8) | decrypted[1];
                double temp = temp10 / 10.0;

                // Humidity is unsigned uint16 in tenths of percent
                uint16_t hum10 = ((uint16_t)decrypted[2] << 8) | decrypted[3];
                double humidity = hum10 / 10.0;

                // Light level is a raw uint16 value (no division)
                uint16_t lightLevel = ((uint16_t)decrypted[4] << 8) | decrypted[5];

                String batteyState = decrypted[6] == 0x00 ? "Battery OK" : "Low battery"; // Assuming the seventh byte of the payload is battery voltage
                Serial.println("Temperature: " + String(temp) + " °C");
                Serial.println("Humidity: " + String(humidity) + " %");
                Serial.println("Light Level: " + String(lightLevel));
                Serial.println("Battery State: " + batteyState);

                blynk_send(temp0Pin, temp);
                blynk_send(humidityPin, humidity);
                blynk_send(lightLevelPin, (double)lightLevel);
                blynk_send(batteryPin, batteyState.c_str());

                // Decide whether to command the curtain node (avoid sending duplicate commands)
                uint8_t desiredCmd = 0x00; // 0 = no-op, 0x01=open, 0x02=close
                if (temp >= HOT_THRESHOLD_C && lightLevel >= LIGHT_BRIGHT_THRESHOLD) {
                    desiredCmd = 0x02; // close curtain
                } else if (temp <= COLD_THRESHOLD_C) {
                    desiredCmd = 0x01; // open curtain
                }

                if (desiredCmd != 0x00) {
                    // Map desiredCmd to desired state for comparison with lastCurtainState
                    uint8_t desiredState = (desiredCmd == 0x01) ? 1 : 2;
                    if (lastCurtainState != 0 && lastCurtainState == desiredState) {
                        Serial.println("Curtain already in desired state — no command sent");
                    } else {
                        // Build and send command packet to curtain node
                        unsigned char payload = desiredCmd;
                        size_t cmd_packet_len = 0;
                        unsigned char* cmd_packet = buildPacket(0x02, CURTAIN_NODE_ADDR, counter, &payload, 1, &cmd_packet_len);
                        if (cmd_packet != NULL) {
                            sendMessage(cmd_packet, cmd_packet_len);
                            counter++;
                            free(cmd_packet);
                            Serial.printf("Sent curtain command 0x%02X to node 0x%02X\n", desiredCmd, CURTAIN_NODE_ADDR);
                        } else {
                            Serial.println("Failed to build curtain command packet");
                        }
                    }
                }
                break;
            }
            case 0x0A: {
                Serial.println("Handling door command packet...");
                door_state = (data_len > 0 && data[0] == 0x01) ? "open" : "closed";
                Serial.println("Door state updated to: " + door_state);
                blynk_send(doorPin, door_state.c_str());

                // If door was locked/closed (user leaving), instruct curtain to close
                if (door_state == "closed") {
                    uint8_t desiredCmd = 0x02; // close curtain
                    uint8_t desiredState = 2;
                    if (lastCurtainState != 0 && lastCurtainState == desiredState) {
                        Serial.println("Curtain already closed — no command sent (door event)");
                    } else {
                        unsigned char payload = desiredCmd;
                        size_t cmd_packet_len = 0;
                        unsigned char* cmd_packet = buildPacket(0x02, CURTAIN_NODE_ADDR, counter, &payload, 1, &cmd_packet_len);
                        if (cmd_packet != NULL) {
                            sendMessage(cmd_packet, cmd_packet_len);
                            counter++;
                            free(cmd_packet);
                            Serial.println("Sent curtain CLOSE command due to door lock event");
                        }
                    }
                }
                break;
            }
            case 0x03: {
                // State report packet — could be from light node or curtain node
                // Distinguish by payload length and device type
                if (data_len == 1) {
                    // Light state report (1 byte: 0x00=off, 0x01=on)
                    Serial.println("Light state report received");
                    String lightState = decrypted[0] == 0x01 ? "on" : "off";
                    Serial.println("Light state: " + lightState);
                    blynk_send(lampStatePin, lightState.c_str());
                } else if (data_len == 6 && decrypted[0] == 0x04) {
                    // Curtain state report (6 bytes: device_type | state | temp | humidity)
                    Serial.println("Curtain state report received");
                    uint8_t curtainState = decrypted[1];
                    int16_t temp10 = ((int16_t)decrypted[2] << 8) | decrypted[3];
                    uint16_t hum10 = ((uint16_t)decrypted[4] << 8) | decrypted[5];
                    double temp = temp10 / 10.0;
                    double humidity = hum10 / 10.0;
                    
                    String stateStr = (curtainState == 1) ? "open" : (curtainState == 2) ? "closed" : "unknown";
                    Serial.println("Curtain state: " + stateStr);
                    Serial.printf("Temperature: %.1f°C, Humidity: %.1f%%\n", temp, humidity);
                    
                    blynk_send(curtainStatePin, stateStr.c_str());
                    // Update last known curtain state
                    lastCurtainState = curtainState;
                    // Optionally send temperature/humidity from curtain node to different pins
                    // blynk_send(temp1Pin, temp);
                    // blynk_send(humidityPin, humidity);
                } else {
                    Serial.println("Unknown state report format — ignoring");
                }
                break;
            }
            default: {
                Serial.println("Unknown packet type, throwing out packet");
                break;
            }
        }

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
