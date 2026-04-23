#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "security.h"

// Initialise object pins for RFID reader and servo motor
#define SS_PIN  21
#define RST_PIN 22
#define SERVO_PIN 13

// Packet Settings
#define HEADER_LEN 5
#define MIC_LEN    4
#define MAX_PAYLOAD 16

// Define states
#define STATE_LOCKED   0x00
#define STATE_UNLOCKED 0x01

// Creates objects for RFID reader and servo motor
MFRC522 rfid(SS_PIN, RST_PIN); 
Servo myServo; 

// RFID card UID that is authorized to unlock the door.
byte authorizedUID[] = {0x63, 0x65, 0xBF, 0xF7}; 

uint16_t currentLockState = STATE_LOCKED; // Track state as 16-bit for consistency
uint16_t msg_counter = 0;

void send_state_update(uint16_t state) {
    // Header format: [src, dst, type, counter_hi, counter_lo]
    unsigned char header[5];
    header[0] = 0x01; // Source ID (this node)
    header[1] = 0x00; // Destination ID (gateway)
    header[2] = 0x0A; // Type (e.g., 0x0A for "Access Log")

    // Splitting the 16-bit counter into two 8-bit bytes for the header
    header[3] = (unsigned char)(msg_counter >> 8); 
    header[4] = (unsigned char)(msg_counter & 0xFF);

    // Prepare payload (state is 0 or 1, is treated as uint16_t)
    unsigned char plaintext[2];
    plaintext[0] = (unsigned char)(state >> 8);
    plaintext[1] = (unsigned char)(state & 0xFF);

    // Packet: 5 (hdr) + 2 (data) + 4 (mic) = 11 bytes
    unsigned char packet[11]; 
    
    // Call the encryption function
    int ret = encrypt_and_mic(
        header, 
        plaintext, 
        2, 
        packet + 5, 
        packet + 5 + 2
    );

    if (ret == 0) {
        memcpy(packet, header, 5);
        
        // This is where you would call LoRa.write(packet, 11);
        Serial.printf("State %04X sent securely. Counter is now: %u\n", state, msg_counter);
        
        msg_counter++; // Safely increments up to 65535
    }
  }



void setup() {
  Serial.begin(115200);
  SPI.begin();           // RFID reader uses SPI bus
  rfid.PCD_Init();       // Tells RC522 to wake up and scan magnetic field for tags
  myServo.attach(SERVO_PIN); // Tells ESP32 which pin to send Servo command to
  myServo.write(0);      // Start in "Locked" position (90 is unlocked)
  currentLockState = STATE_LOCKED;

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
  // For loop, that checks UID against authorizedUID
  for (byte i = 0; i < 4; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX); // Print UID in hex format to Serial Monitor
    if (rfid.uid.uidByte[i] != authorizedUID[i]) match = false; // if the UIDs dont match, sets match to false
  }
  Serial.println();

  // UID matches authorizedUID, tells servo to unlock
  if (match) {
    if (currentLockState == STATE_LOCKED) {
      Serial.println("Authorized: Unlocking...");
      // Unlocking gradually to avoid power spike issues, instead of: myServo.write(90);
      for (int pos = 0; pos <= 90; pos += 1) { 
        myServo.write(pos);              
        delay(15);
      }
      currentLockState = STATE_UNLOCKED;
      send_state_update(STATE_UNLOCKED);
    } 
  }
    else {
      Serial.println("Authorized: Locking...");
      for (int pos = 90; pos <= 0; pos -= 1) { 
        myServo.write(pos);              
        delay(15);
      }
      currentLockState = STATE_LOCKED;
      send_state_update(STATE_LOCKED);
    }
  else {
        Serial.println("Access Denied.");
  }

  rfid.PICC_HaltA();      // Stop reading - otherwise reader will keep reading same card and create jitter
  rfid.PCD_StopCrypto1(); // Clears the reader's internal buffer to be ready for the next card
}