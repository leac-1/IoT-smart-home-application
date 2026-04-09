#pragma once

#ifndef NODE_ID
#define NODE_ID 1
#endif



// WiFi
constexpr const char* WIFI_SSID     = "Jons Hjemmebag";
constexpr const char* WIFI_PASSWORD = "ny19981864pass";

// Virtual pins for Blynk
constexpr int VPIN_LED_STATE     = 0; // V0 — reports LED state to app
constexpr int VPIN_LED_OVERRIDE  = 1; // V1 — manual override from app