#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"
#include "join.h"
#include "packetBuilder.h"
#include "security.h"

// Minimal TDMA downlink curtain node.
// Default mode: join -> sync beacon -> sleep until assigned slot -> listen for command -> act -> ack -> sleep.
// Fallback mode: keep the legacy always-on receive loop behind CURTAIN_USE_TDMA_DOWNLINK = false.

RTC_DATA_ATTR uint8_t rtcNodeId = 0;
RTC_DATA_ATTR uint8_t rtcTdmaSlot = 0;
RTC_DATA_ATTR bool rtcJoined = false;
RTC_DATA_ATTR uint16_t rtcPacketCounter = 0;
RTC_DATA_ATTR uint8_t rtcCurtainState = CURTAIN_UNKNOWN;

static Servo servo;
static CurtainState currentCurtainState = CURTAIN_UNKNOWN;

unsigned char* source_address = NULL;

void goToSleep(unsigned long seconds);

static void initLoRa() {
    Serial2.begin(57600, SERIAL_8N1, RN2483_RX_PIN, RN2483_TX_PIN);

    pinMode(RN2483_RST_PIN, OUTPUT);
    digitalWrite(RN2483_RST_PIN, HIGH);
    digitalWrite(RN2483_RST_PIN, LOW);
    delay(200);
    digitalWrite(RN2483_RST_PIN, HIGH);
    delay(1000);

    Serial.println("Waiting for RN2483 boot...");
    String bootMsg = Serial2.readStringUntil('\n');
    Serial.println("Boot message: [" + bootMsg + "]");

    Serial2.println("mac pause");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set mod lora");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set freq 867000000");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set pwr 14");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set sf sf7");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set afcbw 41.7");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set rxbw 125");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set prlen 8");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set crc on");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set iqi off");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set cr 4/5");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set wdt 60000");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set sync 12");
    Serial2.readStringUntil('\n');
    Serial2.println("radio set bw 125");
    Serial2.readStringUntil('\n');

    Serial.println("LoRa ready");
}

static void setCurtainState(CurtainState target) {
    if (target == CURTAIN_UNKNOWN || target == currentCurtainState) {
        return;
    }

    // Physical open/close direction may need calibration depending on how the servo is mounted.
    const int targetAngle = (target == CURTAIN_OPEN) ? SERVO_OPEN_ANGLE : SERVO_CLOSED_ANGLE;
    Serial.printf("Moving curtain to %s (%d deg)\n",
                  (target == CURTAIN_OPEN) ? "OPEN" : "CLOSED", targetAngle);
    servo.write(targetAngle);
    delay(SERVO_SETTLE_MS);
    currentCurtainState = target;
}

static bool sendCurtainStatus(uint8_t lastCommand, uint8_t resultCode, uint8_t reasonCode) {
    uint8_t payload[5];
    payload[0] = 0x04; // curtain device type
    payload[1] = (uint8_t)currentCurtainState;
    payload[2] = lastCommand;
    payload[3] = resultCode;
    payload[4] = reasonCode;

    unsigned char serverAddr = 0x00;
    size_t packetLen = 0;
    unsigned char* packet = build_packet(0x03, payload, &serverAddr, sizeof(payload));
    if (packet == NULL) {
        Serial.println("Failed to build curtain status packet");
        return false;
    }

    packetLen = 5 + sizeof(payload) + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packetLen; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }
    free(packet);

    Serial.println("Sending curtain status packet: " + hexStr);
    Serial2.print("radio tx " + hexStr + "\r\n");
    delay(500);

    String response = "";
    while (Serial2.available()) {
        response += (char)Serial2.read();
    }
    Serial.println("TX response: [" + response + "]");

    Serial2.print("radio rx 0\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    return response.indexOf("ok") >= 0;
}

static bool handleCurtainCommand(uint8_t command) {
    Serial.printf("Received curtain command: 0x%02X\n", command);

    uint8_t resultCode = 0x00;
    uint8_t reasonCode = 0x03; // user/Blynk by default
    bool stateChanged = false;

    switch (command) {
        case 0x01: // open
            if (currentCurtainState != CURTAIN_OPEN) {
                setCurtainState(CURTAIN_OPEN);
                resultCode = 0x01;
                stateChanged = true;
            }
            break;
        case 0x02: // close
            if (currentCurtainState != CURTAIN_CLOSED) {
                setCurtainState(CURTAIN_CLOSED);
                resultCode = 0x01;
                stateChanged = true;
            }
            break;
        case 0x03: // toggle
            if (currentCurtainState == CURTAIN_OPEN) {
                setCurtainState(CURTAIN_CLOSED);
                resultCode = 0x01;
                stateChanged = true;
            } else if (currentCurtainState == CURTAIN_CLOSED) {
                setCurtainState(CURTAIN_OPEN);
                resultCode = 0x01;
                stateChanged = true;
            } else {
                resultCode = 0x02;
            }
            break;
        case 0x04: // request status
            reasonCode = 0x04;
            break;
        default:
            Serial.printf("Unknown command: 0x%02X\n", command);
            resultCode = 0x02;
            break;
    }

    if (stateChanged || command == 0x04 || resultCode == 0x02) {
        sendCurtainStatus(command, resultCode, reasonCode);
    }

    return resultCode != 0x02;
}

static bool receiveCommandDuringWindow(unsigned long windowMs) {
    Serial2.print("radio rx 0\r\n");
    delay(100);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    unsigned long start = millis();
    while (millis() - start < windowMs) {
        if (!Serial2.available()) {
            delay(10);
            continue;
        }

        String response = Serial2.readStringUntil('\n');
        if (response == "ok" || response == "ok\r") {
            continue;
        }

        if (!response.startsWith("radio_rx")) {
            continue;
        }

        int startIdx = response.indexOf("radio_rx") + 9;
        String hexPayload = response.substring(startIdx);
        hexPayload.trim();

        if (hexPayload.length() < 22) {
            Serial.println("Downlink packet too short");
            continue;
        }

        const int rawLen = hexPayload.length() / 2;
        uint8_t rawBytes[64];
        for (int i = 0; i < rawLen && i < 64; i++) {
            rawBytes[i] = strtol(hexPayload.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
        }

        // Destination check: accept node address or broadcast (0x67 is the repo's default broadcast-style fallback).
        const uint8_t destination = rawBytes[1];
        if (destination != assignedNodeId && destination != 0x67) {
            Serial.printf("Packet not for this node (dest=0x%02X)\n", destination);
            continue;
        }

        if (rawBytes[2] != 0x02) {
            Serial.printf("Not a curtain command packet, type=0x%02X\n", rawBytes[2]);
            continue;
        }

        uint8_t plaintext[1];
        if (decrypt_packet(rawBytes, rawLen, plaintext) != 0) {
            Serial.println("Command MIC verification failed — ignoring");
            continue;
        }

        Serial.printf("Decrypted curtain command: 0x%02X\n", plaintext[0]);
        handleCurtainCommand(plaintext[0]);
        return true;
    }

    Serial.println("No curtain command received in slot window");
    return false;
}

static unsigned long computeSleepUntilSlotMs(const NodeConfig& config) {
    if (config.tdmaSlot > config.currentSlotCount) {
        return (config.tdmaSlot - config.currentSlotCount) * config.slotDuration;
    }
    if (config.tdmaSlot < config.currentSlotCount) {
        return (config.cycleTime - config.currentSlotCount * config.slotDuration)
             + config.tdmaSlot * config.slotDuration;
    }
    return 0;
}

static void curtainTdmaLoop() {
    NodeConfig config{};
    config.tdmaSlot = rtcTdmaSlot;

    if (!waitForBeaconWithTDMA(config)) {
        Serial.println("Failed to get TDMA beacon");
        delay(1000);
        return;
    }

    const unsigned long sleepUntilSlotMs = computeSleepUntilSlotMs(config);
    if (sleepUntilSlotMs > DOWNLINK_WAKE_GUARD_MS) {
        const unsigned long preWakeMs = sleepUntilSlotMs - DOWNLINK_WAKE_GUARD_MS;
        if (preWakeMs > MIN_DEEP_SLEEP_MS) {
            goToSleep(preWakeMs / 1000);
            return;
        }
        delay(preWakeMs);
    }

    Serial.println("Wake window reached — listening for curtain downlink");
    receiveCommandDuringWindow(config.slotDuration + DOWNLINK_WAKE_GUARD_MS);

    rtcPacketCounter = get_packet_counter();
    rtcCurtainState = (uint8_t)currentCurtainState;

    unsigned long elapsed = sleepUntilSlotMs + config.slotDuration;
    unsigned long remainingCycle = (config.cycleTime > elapsed) ? (config.cycleTime - elapsed) : 0;
    Serial.printf("Remaining cycle: %lu ms\n", remainingCycle);

    if (remainingCycle > MIN_DEEP_SLEEP_MS) {
        goToSleep(remainingCycle / 1000);
    } else {
        delay(remainingCycle);
    }
}

static void listenForCommandsForever() {
    Serial.println("Fallback always-on receive mode enabled");
    Serial2.print("radio rx 0\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    while (true) {
        yield();
        if (!Serial2.available()) {
            delay(10);
            continue;
        }

        String response = Serial2.readStringUntil('\n');
        if (!response.startsWith("radio_rx")) {
            continue;
        }

        int startIdx = response.indexOf("radio_rx") + 9;
        String hexPayload = response.substring(startIdx);
        hexPayload.trim();
        if (hexPayload.length() < 22) {
            continue;
        }

        const int rawLen = hexPayload.length() / 2;
        uint8_t rawBytes[64];
        for (int i = 0; i < rawLen && i < 64; i++) {
            rawBytes[i] = strtol(hexPayload.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
        }

        if (rawBytes[2] == 0x02) {
            uint8_t plaintext[1];
            if (decrypt_packet(rawBytes, rawLen, plaintext) == 0) {
                handleCurtainCommand(plaintext[0]);
            }
        }

        Serial2.print("radio rx 0\r\n");
        delay(100);
        while (Serial2.available()) Serial2.readStringUntil('\n');
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   Curtain Node Starting");
    Serial.println("========================================");
    Serial.printf("Servo pin: GPIO %d\n", SERVO_PIN);
    Serial.printf("Curtain TDMA mode: %s\n", CURTAIN_USE_TDMA_DOWNLINK ? "enabled" : "fallback always-on");

    source_address = &assignedNodeId;

    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 1000, 2000);
    currentCurtainState = CURTAIN_OPEN;
    servo.write(SERVO_OPEN_ANGLE);

    initLoRa();
    set_packet_counter(rtcPacketCounter);

    if (!rtcJoined) {
        NodeConfig config;
        if (joinNetwork(config)) {
            rtcNodeId = config.nodeId;
            rtcTdmaSlot = config.tdmaSlot;
            rtcJoined = true;
            assignedNodeId = rtcNodeId;
            source_address = &assignedNodeId;
            Serial.printf("Joined network. Node ID: %d, TDMA Slot: %d\n", assignedNodeId, rtcTdmaSlot);
        } else {
            Serial.println("Join failed — retry later");
            goToSleep(10);
            return;
        }
    } else {
        assignedNodeId = rtcNodeId;
        source_address = &assignedNodeId;
        Serial.printf("Resuming — Node ID: %d, TDMA Slot: %d\n", assignedNodeId, rtcTdmaSlot);
    }

    if (rtcCurtainState == CURTAIN_OPEN || rtcCurtainState == CURTAIN_CLOSED) {
        currentCurtainState = (CurtainState)rtcCurtainState;
        servo.write(currentCurtainState == CURTAIN_OPEN ? SERVO_OPEN_ANGLE : SERVO_CLOSED_ANGLE);
    }

    Serial.println("Setup complete");
}

void loop() {
    if constexpr (CURTAIN_USE_TDMA_DOWNLINK) {
        curtainTdmaLoop();
    } else {
        listenForCommandsForever();
    }
}
