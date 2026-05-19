#include <Arduino.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"
#include "packetBuilder.h"
#include "security.h"
#include "join.h"

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

// Sensor and servo objects
static DHT dht(DHT_PIN, DHT11);
static Servo servo;

// RTC-persisted runtime config
RTC_DATA_ATTR uint8_t rtcNodeId = 0;
RTC_DATA_ATTR uint8_t rtcTdmaSlot = 0;
RTC_DATA_ATTR bool rtcJoined = false;
RTC_DATA_ATTR uint16_t rtcPacketCounter = 0;
RTC_DATA_ATTR uint8_t rtcCurtainState = 0;

// Runtime config assigned during join
uint8_t assignedNodeId = 0; // will be set after join
unsigned char* source_address = NULL;

// Curtain state tracking
CurtainState currentCurtainState = CURTAIN_UNKNOWN;
unsigned long lastStatusSentTime = 0;

// Sensor data structure
struct SensorReading {
    float temperatureC;
    float humidityPercent;
    bool valid;
};

// Forward declarations
SensorReading readSensors();
CurtainState decideCurtainState(float temperatureC, CurtainState current);
void moveCurtain(CurtainState target);
bool sendCurtainStateUpdate();
void initLoRa();
void listenForCommands();
void initSensors();
// Power management (copied from sensor node)
void goToSleep(unsigned long seconds);

// ============================================================================
// SENSOR READING
// ============================================================================
SensorReading readSensors() {
    SensorReading reading{};
    
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    // Check for valid readings
    if (isnan(temp) || isnan(hum)) {
        Serial.println("DHT11 read failed — skipping");
        reading.valid = false;
        reading.temperatureC = 0.0;
        reading.humidityPercent = 0.0;
    } else {
        reading.valid = true;
        reading.temperatureC = temp;
        reading.humidityPercent = hum;
        Serial.printf("DHT11: Temp=%.1f°C, Humidity=%.1f%%\n", temp, hum);
    }
    
    return reading;
}

void initSensors() {
    dht.begin();
    delay(1000);
}

// ============================================================================
// CURTAIN STATE DECISION (with hysteresis)
// ============================================================================
CurtainState decideCurtainState(float temperatureC, CurtainState current) {
    // If temp is too hot, close the curtains (clockwise)
    if (temperatureC >= HOT_THRESHOLD_C) {
        return CURTAIN_CLOSED;
    }
    
    // If temp is too cold, open the curtains (counter-clockwise)
    if (temperatureC <= COLD_THRESHOLD_C) {
        return CURTAIN_OPEN;
    }
    
    // In between thresholds — maintain current state (hysteresis)
    return current;
}

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
bool sendCurtainStateUpdate(uint8_t reasonCode = 0x00) {
    // Read fresh sensor data
    SensorReading reading = readSensors();
    
    if (!reading.valid) {
        Serial.println("Sensor reading invalid — will not send packet");
        return false;
    }

    // Build 7-byte payload:
    // Byte 0: device type = 0x04 for curtain node
    // Byte 1: curtain state (0=unknown, 1=open, 2=closed)
    // Byte 2-3: temperature × 10 (signed int16, big-endian)
    // Byte 4-5: humidity × 10 (unsigned int16, big-endian)
    // Byte 6: reason code

    uint8_t payload[7];
    payload[0] = 0x04;  // Device type: curtain node
    payload[1] = (uint8_t)currentCurtainState;  // State

    int16_t temp10 = (int16_t)(reading.temperatureC * 10.0);
    payload[2] = (temp10 >> 8) & 0xFF;      // Temperature high byte
    payload[3] = temp10 & 0xFF;             // Temperature low byte

    uint16_t hum10 = (uint16_t)(reading.humidityPercent * 10.0);
    payload[4] = (hum10 >> 8) & 0xFF;       // Humidity high byte
    payload[5] = hum10 & 0xFF;              // Humidity low byte

    payload[6] = reasonCode;

    // Stop receiving and prepare for transmission
    Serial2.print("radio rxstop\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    // Build encrypted packet (packet type 0x03 for state report)
    unsigned char dst = 0x00;  // Destination: server/gateway
    unsigned char* packet = build_packet(0x03, payload, &dst, 7);

    if (packet == NULL) {
        Serial.println("Failed to build curtain state packet");
        return false;
    }

    // Convert packet to hex string for transmission
    size_t packet_len = 5 + 7 + 4 + 1;  // header + payload + MIC + CRC
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }

    free(packet);

    Serial.println("Sending curtain state packet: " + hexStr);
    Serial2.print("radio tx " + hexStr + "\r\n");
    delay(500);

    // Check response
    String response = "";
    while (Serial2.available()) {
        response += (char)Serial2.read();
    }

    Serial.println("TX response: [" + response + "]");

    // Return to receive mode
    Serial2.print("radio rx 0\r\n");
    delay(200);
    while (Serial2.available()) Serial2.readStringUntil('\n');

    lastStatusSentTime = millis();
    return response.indexOf("ok") >= 0;
}

// ============================================================================
// COMMAND RECEPTION (optional server commands)
// ============================================================================
void handleCurtainCommand(uint8_t command) {
    Serial.printf("Received curtain command: 0x%02X\n", command);
    
    switch (command) {
        case 0x01:  // Open
            Serial.println("Command: OPEN");
            moveCurtain(CURTAIN_OPEN);
            break;
        case 0x02:  // Close
            Serial.println("Command: CLOSE");
            moveCurtain(CURTAIN_CLOSED);
            break;
        case 0x03:  // Toggle
            Serial.println("Command: TOGGLE");
            if (currentCurtainState == CURTAIN_OPEN) {
                moveCurtain(CURTAIN_CLOSED);
            } else if (currentCurtainState == CURTAIN_CLOSED) {
                moveCurtain(CURTAIN_OPEN);
            } else {
                Serial.println("Cannot toggle from UNKNOWN state");
            }
            break;
        case 0x04:  // Request status
            Serial.println("Command: REQUEST STATUS");
            break;
        default:
            Serial.printf("Unknown command: 0x%02X — ignoring\n", command);
            return;
    }
    
    sendCurtainStateUpdate();
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
                    uint8_t rawBytes[11];
                    for (int i = 0; i < 11; i++) {
                        rawBytes[i] = strtol(hexPayload.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
                    }
                    
                    uint8_t plaintext[1];
                    if (decrypt_packet(rawBytes, 11, plaintext) != 0) {
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
    source_address = &assignedNodeId; // will be updated after join
    Serial.println("Initializing DHT11 sensor...");
    initSensors();

    Serial.println("Initializing servo motor...");
    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 1000, 2000);

    // Initialize LoRa
    Serial.println("Initializing LoRa communication...");
    initLoRa();

    // Restore RTC state
    set_packet_counter(rtcPacketCounter);

    if (!rtcJoined) {
        NodeConfig config;
        if (joinNetwork(config)) {
            rtcNodeId = config.nodeId;
            rtcTdmaSlot = config.tdmaSlot;
            rtcJoined = true;
            assignedNodeId = rtcNodeId;
            source_address = &assignedNodeId;
            Serial.printf("Joined network. Node ID: %d, Slot: %d\n", assignedNodeId, rtcTdmaSlot);
        } else {
            Serial.println("Join failed — going to sleep and retrying later");
            goToSleep(10);
            return;
        }
    } else {
        assignedNodeId = rtcNodeId;
        source_address = &assignedNodeId;
        Serial.printf("Resuming — Node ID: %d, Slot: %d\n", rtcNodeId, rtcTdmaSlot);
    }

    // Restore curtain state if available
    if (rtcCurtainState == CURTAIN_OPEN || rtcCurtainState == CURTAIN_CLOSED) {
        currentCurtainState = (CurtainState)rtcCurtainState;
        int angle = (currentCurtainState == CURTAIN_OPEN) ? SERVO_OPEN_ANGLE : SERVO_CLOSED_ANGLE;
        servo.write(angle);
    } else {
        currentCurtainState = CURTAIN_OPEN;
        servo.write(SERVO_OPEN_ANGLE);
    }

    Serial.println("Setup complete — entering TDMA loop");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    NodeConfig config;
    config.tdmaSlot = rtcTdmaSlot;

    if (!waitForBeaconWithTDMA(config)) {
        Serial.println("Failed to get TDMA beacon");
        delay(1000);
        return;
    }

    unsigned long sleepUntilSlot = 0;
    if (config.tdmaSlot > config.currentSlotCount) {
        sleepUntilSlot = (config.tdmaSlot - config.currentSlotCount) * config.slotDuration;
    } else if (config.tdmaSlot < config.currentSlotCount) {
        sleepUntilSlot = (config.cycleTime - config.currentSlotCount * config.slotDuration)
                       + config.tdmaSlot * config.slotDuration;
    }

    if (sleepUntilSlot > 0) delay(sleepUntilSlot);

    // Read sensors and decide
    SensorReading data = readSensors();
    if (!data.valid) {
        Serial.println("Invalid sensor reading — skipping action this slot");
    } else {
        CurtainState target = decideCurtainState(data.temperatureC, currentCurtainState);
        uint8_t reason = 0x03; // no change/status only
        if (target != currentCurtainState) {
            if (target == CURTAIN_CLOSED) reason = 0x01; // closed because too hot
            else if (target == CURTAIN_OPEN) reason = 0x02; // opened because too cold
            moveCurtain(target);
        }

        // Send state update (with reason)
        bool ok = sendCurtainStateUpdate(reason);
        if (ok) {
            Serial.println("Curtain state update sent");
        } else {
            Serial.println("Failed to send curtain state update");
        }
    }

    // Persist packet counter and curtain state to RTC
    rtcPacketCounter = get_packet_counter();
    rtcCurtainState = (uint8_t)currentCurtainState;

    unsigned long timeUsed = sleepUntilSlot + config.slotDuration;
    unsigned long remainingCycle = 0;
    if (config.cycleTime > timeUsed) {
        remainingCycle = config.cycleTime - timeUsed;
    }

    Serial.printf("Remaining cycle: %lu ms\n", remainingCycle);

    if (remainingCycle > 10000) {
        // Sleep for remaining cycle (convert to seconds)
        goToSleep(remainingCycle / 1000);
    } else {
        delay(remainingCycle);
    }
}
