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
    pinMode(RST, OUTPUT);
    digitalWrite(RST, LOW);
    delay(1);
    digitalWrite(RST, HIGH);
    delay(100); // give time to boot
}

void sendMessage(unsigned char* fullPacket, size_t packetLength) {
    if (fullPacket == NULL || packetLength == 0) {
        return;
    }
    
    String packetString = "";
    for (size_t i = 0; i < packetLength; i++) {
        char hexByte[3];
        sprintf(hexByte, "%02X", fullPacket[i]);
        packetString += hexByte;
    }

    loraSerial.print("radio tx " + packetString); //Send commands to RN
    loraSerial.print("\r\n"); //Newline
    delay(200); //Delay so the RN can response
    String response = "";
    //Read response from RN and print in serial monitor
    while (loraSerial.available()) {
        response += (char)loraSerial.read();
    }
    Serial.println(response);
    loraSerial.print("radio rx 0");
    loraSerial.print("\r\n");
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

