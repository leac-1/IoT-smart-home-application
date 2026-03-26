#include <Arduino.h>
#include "pins.h"
#include "secrets.h"

// RN2483 communicates over UART via ASCII commands
// Serial2 uses GPIO18 (TX) and GPIO19 (RX) on the ESP32

void initLoRa() {
    Serial2.begin(57600, SERIAL_8N1, RN2483_RX_PIN, RN2483_TX_PIN);

    // Hard reset the module
    pinMode(RN2483_RST_PIN, OUTPUT);
    digitalWrite(RN2483_RST_PIN, LOW);
    delay(10);
    digitalWrite(RN2483_RST_PIN, HIGH);
    delay(500); // Wait for module to boot

    // TODO: OTAA join sequence using DEV_EUI, APP_EUI, APP_KEY
    // e.g. Serial2.println("mac set deveui " + String(DEV_EUI));
    //      Serial2.println("mac set appeui " + String(APP_EUI));
    //      Serial2.println("mac set appkey " + String(APP_KEY));
    //      Serial2.println("mac join otaa");
}

bool sendPayload(const uint8_t* payload, size_t length) {
    // TODO: convert payload bytes to hex string and transmit
    // e.g. Serial2.println("mac tx uncnf 1 <hexstring>");
    return true;
}