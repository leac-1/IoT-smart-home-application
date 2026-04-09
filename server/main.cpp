#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "packetManager.cpp"
#include "LoraCom.cpp"

#define RXD2 18
#define TXD2 19
#define RST 23

unsigned char* source_address = NULL;

void setup() {
    source_address = (unsigned char*)malloc(1);
    if (source_address != NULL) {
        *source_address = 0x00;
    }

    Serial.begin(115200);

    setupLora();
}

void loop() {
    static unsigned char counter = 0;
    unsigned char destination = 0x00;
    size_t packetLength = 0;
    
    unsigned char data[] = "Hello, LoRa!";
    unsigned char* packet = build_packet(0x01, data, sizeof(data) - 1, &destination, counter, &packetLength);

    if (packet == NULL) {
        Serial.println("Failed to build packet");
        delay(5000);
        return;
    }

    Serial.println("Packet built");
    String packetString = "";
    for (size_t i = 0; i < packetLength; i++) {
        char hexByte[3];
        sprintf(hexByte, "%02X", packet[i]);
        packetString += hexByte;
    }
    Serial.println("Packet String: " + packetString);

    sendMessage(packet, packetLength);
    free(packet);

    Serial.print("Packet sent with counter: ");
    Serial.println(counter);
    counter++;
    delay(5000); // Wait for 5 seconds before sending the next packet
}

extern "C" void app_main(void) {
    initArduino();

    setup();

    while (true) {
        loop();
    }
}
