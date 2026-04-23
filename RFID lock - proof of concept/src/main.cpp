#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// Initialise object pins for RFID reader and servo motor

#define SS_PIN  21
#define RST_PIN 22
#define SERVO_PIN 13

// Creates objects for RFID reader and servo motor
MFRC522 rfid(SS_PIN, RST_PIN); 
Servo myServo; 

// Replace this with your actual tag UID after scanning
byte authorizedUID[] = {0x63, 0x65, 0xBF, 0xF7}; 

void setup() {
  Serial.begin(115200);
  SPI.begin();           // RFID reader uses SPI bus
  rfid.PCD_Init();       // Tells RC522 to wake up and scan magnetic field for tags
  myServo.attach(SERVO_PIN); // Tells ESP32 which pin to send Servo command to
  myServo.write(0);      // Start in "Locked" position (90 is unlocked)
  Serial.println("System Ready. Scan your card...");
}

void loop() {
  // Look for present tags (RFID cards)
  // Loop starts over if no card is present
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  // If a card is present, read its UID. Loop starts over if reading fails
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("Card UID:");
  bool match = true; // assumes match until proven otherwise
  // For loop checks UID against authorizedUID
  for (byte i = 0; i < 4; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX); // Print UID in hex format to Serial Monitor
    if (rfid.uid.uidByte[i] != authorizedUID[i]) match = false; // if the UIDs dont match, sets match to false
  }
  Serial.println();

  // UID matches authorizedUID, tells servo to unlock
  if (match) {
    Serial.println("Access Granted!");
    // Unlocking gradually to avoid power spike issues, instead of: myServo.write(90);
    for (int pos = 0; pos <= 90; pos += 1) { 
      myServo.write(pos);              
      delay(15);
    }
    delay(5000);         // Waits 5 seconds
    myServo.write(0);    // Relocks after the 5 seconds
  } 
  // if Card doesnt match, prints Access Denied to Serial Monitor and does not unlock
  else {
    Serial.println("Access Denied."); 
  }

  rfid.PICC_HaltA();      // Stop reading - otherwise reader will keep reading same card and create jitter
  rfid.PCD_StopCrypto1(); // Clears the reader's internal buffer to be ready for the next card
}