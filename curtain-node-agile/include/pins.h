#pragma once

// DHT11 sensor pin (GPIO number, not board pin)
constexpr int DHT_PIN = 4;

// SG90 servo signal pin (GPIO number)
constexpr int SERVO_PIN = 13;

// RN2483/RN2384 LoRa module UART pins
constexpr int RN2483_TX_PIN = 18;
constexpr int RN2483_RX_PIN = 19;
constexpr int RN2483_RST_PIN = 23;

// Servo control angles (adjustable via calibration)
constexpr int SERVO_OPEN_ANGLE = 0;      // Fully open (counter-clockwise)
constexpr int SERVO_CLOSED_ANGLE = 90;   // Fully closed (clockwise)
constexpr int SERVO_SETTLE_MS = 700;     // Time to allow servo to settle before sending packet
