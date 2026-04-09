#pragma once

// LED
constexpr int LED_PIN = 2;

// RN2483 UART
constexpr int RN2483_TX_PIN = 18; // ESP32 TX → RN2483 RX
constexpr int RN2483_RX_PIN = 19; // ESP32 RX → RN2483 TX
constexpr int RN2483_RST_PIN = 23;