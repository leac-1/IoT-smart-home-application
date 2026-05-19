#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"
#include "packetBuilder.h"
#include "security.h"
// Actuator-only node (always-on) — reuse packetBuilder/security for crypto and framing

/*
 * Curtain Node for IoT Smart Home
 * 
 * Hardware:
 * - ESP32 microcontroller
 * - RN2483/RN2384 LoRa module on Serial2 (UART)
 * - DHT11 temperature/humidity sensor on GPIO 4
 * - TowerPro SG90 servo motor on GPIO 13 (PWM)
 * 
 * Behavior:
 * - Reads DHT11 every TDMA cycle
 * - Uses hysteresis: close if temp >= 26°C, open if temp <= 20°C
 * - Sends encrypted state update to server after servo movement
 * - Uses hardcoded Node ID (similar to light node)
 * 
 * Packet Types:
 * 0x03 — Curtain state report (node → server)
 *        Payload: [device_type(1)] | [state(1)] | [temp×10(2)] | [humidity×10(2)]
 * 0x02 — Curtain command (server → node, if implemented)
 *        Payload: [command(1)] where 0x01=open, 0x02=close, 0x03=toggle, 0x04=status
 */

static Servo servo;
static Servo servo;

// Runtime config
uint8_t assignedNodeId = 6; // hardcoded curtain node ID
unsigned char* source_address = NULL;

// Curtain state tracking
CurtainState currentCurtainState = CURTAIN_UNKNOWN;

// Forward declarations
void moveCurtain(CurtainState target);
bool sendCurtainStateUpdate(uint8_t lastCommand, uint8_t actionResult, uint8_t reasonCode);
void initLoRa();
void listenForCommands();

// No onboard sensors in Version 2 — server issues commands based on central logic

// ============================================================================
// SERVO CONTROL
// ============================================================================
void moveCurtain(CurtainState target) {
    if (target == CURTAIN_UNKNOWN) {
        Serial.println("Cannot move to UNKNOWN state");
        return;
    }
    
    if (target == currentCurtainState) {
        Serial.println("Curtain already in target state — no movement needed");
        return;
    }
    
    int targetAngle = (target == CURTAIN_OPEN) ? SERVO_OPEN_ANGLE : SERVO_CLOSED_ANGLE;
    const char* directionStr = (target == CURTAIN_OPEN) ? "OPEN (counter-clockwise)" : "CLOSED (clockwise)";
    
    Serial.printf("Moving servo to %s (%d°)...\n", directionStr, targetAngle);
    servo.write(targetAngle);
    
    // Allow servo to settle before sending packet
    delay(SERVO_SETTLE_MS);
    
    currentCurtainState = target;
    Serial.printf("Curtain is now %s\n", directionStr);
}

// ============================================================================
// PACKET CONSTRUCTION & TRANSMISSION
// ============================================================================
// Send curtain state report back to server
// payload format (5 bytes):
// Byte 0: device type = 0x04
// Byte 1: curtain state (0 unknown, 1 open, 2 closed)
// Byte 2: last command received (0x01=open,0x02=close,0x03=toggle,0x04=status)
// Byte 3: action result (0x00=no change,0x01=changed,0x02=invalid,0x03=failed)
// Byte 4: reason/source code (see spec)
bool sendCurtainStateUpdate(uint8_t lastCommand, uint8_t actionResult, uint8_t reasonCode) {
    uint8_t payload[5];
    payload[0] = 0x04;
    payload[1] = (uint8_t)currentCurtainState;
    payload[2] = lastCommand;
    payload[3] = actionResult;
    payload[4] = reasonCode;

    // Stop receiving and prepare for transmission
    Serial2.print("radio rxstop\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    unsigned char dst = 0x00; // server/gateway
    unsigned char* packet = build_packet(0x03, payload, &dst, 5);
    if (packet == NULL) {
        Serial.println("Failed to build curtain status packet");
        return false;
    }

    size_t packet_len = 5 + 5 + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }
    free(packet);

    Serial.println("Sending curtain status packet: " + hexStr);
    Serial2.print("radio tx " + hexStr + "\r\n");
    delay(500);

    String response = "";
    while (Serial2.available()) response += (char)Serial2.read();
    Serial.println("TX response: [" + response + "]");

    Serial2.print("radio rx 0\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    return response.indexOf("ok") >= 0;
}

// ============================================================================
// COMMAND RECEPTION (optional server commands)
// ============================================================================
void handleCurtainCommand(uint8_t command) {
    Serial.printf("Received curtain command: 0x%02X\n", command);

    uint8_t actionResult = 0x00; // default: no change needed
    uint8_t reason = 0x03; // assume Blynk/user command by default

    switch (command) {
        case 0x01:  // Open
            Serial.println("Command: OPEN");
            if (currentCurtainState != CURTAIN_OPEN) {
                moveCurtain(CURTAIN_OPEN);
                actionResult = 0x01; // changed successfully
            } else {
                actionResult = 0x00; // no change needed
            }
            break;
        case 0x02:  // Close
            Serial.println("Command: CLOSE");
            if (currentCurtainState != CURTAIN_CLOSED) {
                moveCurtain(CURTAIN_CLOSED);
                actionResult = 0x01;
            } else {
                actionResult = 0x00;
            }
            break;
        case 0x03:  // Toggle
            Serial.println("Command: TOGGLE");
            if (currentCurtainState == CURTAIN_OPEN) {
                moveCurtain(CURTAIN_CLOSED);
                actionResult = 0x01;
            } else if (currentCurtainState == CURTAIN_CLOSED) {
                moveCurtain(CURTAIN_OPEN);
                actionResult = 0x01;
            } else {
                Serial.println("Cannot toggle from UNKNOWN state");
                actionResult = 0x02; // invalid
            }
            break;
        case 0x04:  // Request status
            Serial.println("Command: REQUEST STATUS");
            actionResult = 0x00;
            reason = 0x04;
            break;
        default:
            Serial.printf("Unknown command: 0x%02X — ignoring\n", command);
            actionResult = 0x02; // invalid command
            break;
    }

    // Send acknowledgment/status back to server
    sendCurtainStateUpdate(command, actionResult, reason);
}

void listenForCommands() {
    Serial.println("Listening for commands...");
    Serial2.print("radio rxstop\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');
    
    Serial2.print("radio rx 0\r\n");
    delay(500);
    String rxResponse = Serial2.readStringUntil('\n');
    Serial.println("RX mode response: " + rxResponse);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    while (true) {
        yield();
        delay(50);

        if (Serial2.available()) {
            String response = Serial2.readStringUntil('\n');

            if (response == "ok" || response == "ok\r") continue;

            Serial.println("Radio received: " + response);

            if (response.indexOf("radio_rx") >= 0) {
                int startIdx = response.indexOf("radio_rx") + 9;
                String hexPayload = response.substring(startIdx);
                hexPayload.trim();

                Serial.println("Hex payload: " + hexPayload);

                // Parse packet type at bytes 4-5
                String packetType = hexPayload.substring(4, 6);
                if (packetType == "02") {  // Command packet (0x02)
                    Serial.println("Command packet detected — attempting to decrypt");
                    
                    // Convert hex string to raw bytes
                    const int raw_len = hexPayload.length() / 2;
                    uint8_t rawBytes[64];
                    for (int i = 0; i < raw_len; i++) {
                        rawBytes[i] = strtol(hexPayload.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
                    }
                    
                    uint8_t plaintext[1];
                    if (decrypt_packet(rawBytes, raw_len, plaintext) != 0) {
                        Serial.println("Command MIC verification failed — ignoring");
                    } else {
                        handleCurtainCommand(plaintext[0]);
                    }
                } else {
                    Serial.println("Not a command packet, type: " + packetType + " — ignoring");
                }
                
                // Re-arm receiver
                Serial2.print("radio rx 0\r\n");
                delay(200);
                while (Serial2.available()) Serial2.readStringUntil('\n');
            }
        }
    }
}

// ============================================================================
// LORA INITIALIZATION
// ============================================================================
void initLoRa() {
    Serial2.begin(57600, SERIAL_8N1, RN2483_RX_PIN, RN2483_TX_PIN);
    
    pinMode(RN2483_RST_PIN, OUTPUT);
    digitalWrite(RN2483_RST_PIN, HIGH);
    digitalWrite(RN2483_RST_PIN, LOW);
    delay(200);
    digitalWrite(RN2483_RST_PIN, HIGH);
    delay(1000);
    
    Serial.println("Waiting for RN2483 boot...");
    String str = Serial2.readStringUntil('\n');
    Serial.println("Boot message: [" + str + "]");

    Serial2.println("mac pause");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set mod lora");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set freq 867000000");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set pwr 14");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set sf sf7");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set afcbw 41.7");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set rxbw 125");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set prlen 8");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set crc on");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set iqi off");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set cr 4/5");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set wdt 60000");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set sync 12");
    str = Serial2.readStringUntil('\n');

    Serial2.println("radio set bw 125");
    str = Serial2.readStringUntil('\n');

    Serial.println("LoRa ready");
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================");
    Serial.println("   Curtain Node Starting");
    Serial.println("========================================");
    Serial.printf("DHT11 pin: GPIO %d\n", DHT_PIN);
    Serial.printf("Servo pin: GPIO %d\n", SERVO_PIN);
    Serial.printf("Temperature thresholds: %.1f°C (cold) — %.1f°C (hot)\n", COLD_THRESHOLD_C, HOT_THRESHOLD_C);

    // Initialize hardware
    source_address = &assignedNodeId;

    Serial.println("Initializing servo motor...");
    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 1000, 2000);
    // Start with curtains open by default
    currentCurtainState = CURTAIN_OPEN;
    servo.write(SERVO_OPEN_ANGLE);

    // Initialize LoRa
    Serial.println("Initializing LoRa communication...");
    initLoRa();

    Serial.printf("Curtain node ready — Node ID: %d\n", assignedNodeId);

    // Enter continuous receive loop (always-on actuator)
    listenForCommands();
}

void loop() {
    // Nothing here — listenForCommands() is blocking
    delay(1000);
}
