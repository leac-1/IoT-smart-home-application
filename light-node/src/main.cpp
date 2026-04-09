#include <Arduino.h>
#include "config.h"
#include "packet.h"

// Blynk
#define BLYNK_TEMPLATE_ID "TMPL5MkNq40p3"
#define BLYNK_TEMPLATE_NAME "Lightbulb"
#define BLYNK_AUTH_TOKEN "Ha0D0GLO42pU44HxynEMGjN4IxpqQig8"


// Declared in led.cpp
void initLed();
void setLed(bool on);

// Declared in lora.cpp
void initLoRa();
bool receivePacket(IncomingPacket& packet);

// Declared in blynk.cpp
void initBlynk();
void updateBlynk();
void runBlynk();


void setup() {
    Serial.begin(115200);
    delay(1000);

    initLed();
    // initLoRa();
    initBlynk();

    Serial.println("Lightbulb ready");
}


void loop() {
    runBlynk(); // handles Blynk connection and incoming commands

    // Commented out for testing without LoRa
    /*
    IncomingPacket packet;
    if (receivePacket(packet)) {
        if (packet.nodeId == NODE_ID) {
            setLed(packet.command == 0x01);
            updateBlynk();

            Serial.print("Command received: ");
            Serial.println(packet.command == 0x01 ? "ON" : "OFF");
        }
    }
        */
}