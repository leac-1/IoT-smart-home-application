#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "packetBuilder.h"

void initLoRa() {
    Serial2.begin(57600, SERIAL_8N1, RN2483_RX_PIN, RN2483_TX_PIN);

    pinMode(RN2483_RST_PIN, OUTPUT);
    digitalWrite(RN2483_RST_PIN, HIGH);
    digitalWrite(RN2483_RST_PIN, LOW);
    delay(200);
    digitalWrite(RN2483_RST_PIN, HIGH);
    delay(500);

    String str = Serial2.readStringUntil('\n');
    Serial.println(str);

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

bool sendPayload(const uint8_t* payload, size_t length) {
    Serial2.print("radio rxstop\r\n");
    delay(500);
    Serial2.readStringUntil('\n');
    Serial2.readStringUntil('\n');

    unsigned char dst = 0x00;

    unsigned char* packet = build_packet(0x01, payload, &dst, length);
    if (packet == NULL) {
        Serial.println("Failed to build packet");
        return false;
    }

    size_t packet_len = 5 + length + 4 + 1;
    String hexStr = "";
    for (size_t i = 0; i < packet_len; i++) {
        if (packet[i] < 0x10) hexStr += "0";
        hexStr += String(packet[i], HEX);
    }

    free(packet);

    Serial2.print("radio tx " + hexStr);
    Serial2.print("\r\n");
    delay(200);

    String response = "";
    while (Serial2.available()) {
        response += (char)Serial2.read();
    }
    Serial.println("RN2483: " + response);

    return response.indexOf("ok") >= 0;
}