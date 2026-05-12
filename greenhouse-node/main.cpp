#include <Arduino.h>

extern bool lorawan_setup();
extern bool lorawan_send(const unsigned char* data, size_t data_len, int port);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Greenhouse Node ===");

    if (lorawan_setup()) {
        Serial.println("Joined Cibicom");
    } else {
        Serial.println("Join failed");
    }
}

void loop() {
    unsigned char payload[2] = {0xDB, 0x00};
    lorawan_send(payload, 2, 1);
    Serial.println("Sent 21.9C to Cibicom");
    delay(120000);
}
