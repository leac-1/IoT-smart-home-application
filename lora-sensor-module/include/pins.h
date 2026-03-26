/** Pin Definitions */

#pragma once 

// DHT11
constexpr int DHT_PIN = 4; //GPIO4 (D4) 

// Photocell
constexpr int LDR_PIN = 2; //GPIO2 (D2)

// LoRa
constexpr int RN2483_TX_PIN = 18; // ESP32 TX → RN2483 RX
constexpr int RN2483_RX_PIN = 19; // ESP32 RX → RN2483 TX
constexpr int RN2483_RST_PIN = 23;