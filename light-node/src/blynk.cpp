#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

#define BLYNK_TEMPLATE_ID "TMPL5MkNq40p3"
#define BLYNK_TEMPLATE_NAME "Lightbulb"
#define BLYNK_AUTH_TOKEN "Ha0D0GLO42pU44HxynEMGjN4IxpqQig8"
#include <BlynkSimpleEsp32.h>

// Forward declaration from led.cpp
void setLed(bool on);
bool getLedState();

void runBlynk() {
    Blynk.run();
}

void initBlynk() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
}

void updateBlynk() {
    Blynk.virtualWrite(VPIN_LED_STATE, getLedState() ? 1 : 0);
}

// Called when manual override button is pressed in Blynk app
BLYNK_WRITE(V1) {
    int value = param.asInt();
    setLed(value == 1);
    updateBlynk();
}