#include "Arduino.h"
#include "HardwareSerial.h"


#define RXD2 16
#define TXD2 17
#define RST 25

HardwareSerial loraSerial(1);
String str;

void sleepLora() {
    loraSerial.println("sys sleep 500"); // sleep 500ms
}

void wakeupLora() {
    loraSerial.print("\r\n");
    delay(200); // give more time to wake to avoid bleeding into the command
    while (loraSerial.available()) loraSerial.read(); // flush any response
}

void sendMessage(unsigned char* fullPacket, size_t packetLength) {
    if (fullPacket == NULL || packetLength == 0) {
        return;
    }
    
    // Stop any ongoing receive before transmitting
    loraSerial.print("radio rxstop\r\n");
    delay(100);
    while (loraSerial.available()) loraSerial.read(); // flush response

    String packetString = "";
    for (size_t i = 0; i < packetLength; i++) {
        char hexByte[3];
        sprintf(hexByte, "%02X", fullPacket[i]);
        packetString += hexByte;
    }

    loraSerial.print("radio tx " + packetString + "\r\n");
    
    String response1 = loraSerial.readStringUntil('\n'); // "ok"
    Serial.println("TX 1: " + response1);
    
    String response2 = loraSerial.readStringUntil('\n'); // "radio_tx_ok"
    Serial.println("TX 2: " + response2);
}


void setupLora(){
    loraSerial.begin(57600, SERIAL_8N1, RXD2, TXD2);
    // Reset RN2483
    pinMode(RST, OUTPUT);
    digitalWrite(RST, HIGH);
    digitalWrite(RST, LOW);
    delay(200);
    digitalWrite(RST, HIGH);
    delay(500);
    Serial.println("Initing LoRa");
    //Read startup message from RN
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    loraSerial.println("sys get ver");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Pause the MAC layer so the RN can operate in LoRa P2P
    loraSerial.println("mac pause");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Modulation set to LoRa
    loraSerial.println("radio set mod lora");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Set the frq between 863 and 867 MHz
    loraSerial.println("radio set freq 867000000");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Power in dBm. Max reach 14
    loraSerial.println("radio set pwr 14");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    loraSerial.println("radio set sf sf7");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //AFC is set to 41.7
    loraSerial.println("radio set afcbw 41.7");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Receiver bandwidth set to 125 kHz
    loraSerial.println("radio set rxbw 125");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Preamble length
    loraSerial.println("radio set prlen 8");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //CRC error checking
    loraSerial.println("radio set crc on");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    loraSerial.println("radio set iqi off");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //code rating 4/5
    loraSerial.println("radio set cr 4/5");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //Watchdog timer
    loraSerial.println("radio set wdt 60000"); //disable for continuous reception
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //HEX value
    loraSerial.println("radio set sync 12");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    //signal bandwirth 125 kHz
    loraSerial.println("radio set bw 125");
    str = loraSerial.readStringUntil('\n');
    Serial.println(str);
    Serial.println("starting loop");
}