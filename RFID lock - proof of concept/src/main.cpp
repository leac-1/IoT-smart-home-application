#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "security.h"
#include "LoraCom.h"

unsigned char my_address = 0x01; 
unsigned char* source_address = &my_address;

// --- RN2483 Serial Settings ---
#define RXD2 18
#define TXD2 19
#define RN_RST 22
HardwareSerial loraSerial(2); // Use Serial2 for the RN2483

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

unsigned char* build_packet(unsigned char type, unsigned char* data, size_t data_len, const unsigned char* des, uint16_t msg_counter, size_t* out_packet_len) {
    if (data == NULL) return NULL;

    // 1. Build the 5-byte header [src, dst, type, count_hi, count_lo]
    unsigned char* header = build_header(type, des, msg_counter);
    if (header == NULL) return NULL;

    const size_t header_len = 5; // Updated: no preamble
    size_t packet_size = header_len + data_len + 4 + 1; // Hdr + Data + MIC + CRC

    unsigned char* packet = (unsigned char*)malloc(packet_size);
    if (packet == NULL) { free(header); return NULL; }

    // 2. Copy header to the start of packet
    memcpy(packet, header, header_len);

    // 3. ENCRYPT & MIC
    // enc_out points to packet[5], mic_out points to packet[5 + data_len]
    int res = encrypt_and_mic(
        header, 
        data, 
        data_len, 
        packet + header_len,           // Ciphertext output
        packet + header_len + data_len // MIC output
    );

    if (res != 0) {
        free(header);
        free(packet);
        return NULL;
    }

    // 4. Calculate CRC on the encrypted data
    unsigned char* crc = CRC8(packet + header_len); // Run CRC on ciphertext
    if (crc == NULL) {
        free(header);
        free(packet);
        return NULL;
    }
    packet[packet_size - 1] = crc[0];

    if (out_packet_len != NULL) *out_packet_len = packet_size;

    free(header);
    free(crc);
    return packet;
}

void send_state_update(uint16_t state) {
    unsigned char data[2] = { (unsigned char)(state >> 8), (unsigned char)(state & 0xFF) };
    size_t packet_len;
    unsigned char server_addr = 0x00; // Matching server's source_address = 0x00

    unsigned char* packet = build_packet(0x0A, data, 2, &server_addr, msg_counter, &packet_len);
    
    if (packet != NULL) {
        sendMessage(packet, packet_len);
        Serial.printf("State %02X sent to Server. Counter: %u\n", state, msg_counter);
        
        free(packet); 
        msg_counter++; 
    }
}

void setup() {
  Serial.begin(115200);
  SPI.begin();           // RFID reader uses SPI bus
  rfid.PCD_Init();       // Tells RC522 to wake up and scan magnetic field for tags
  myServo.attach(SERVO_PIN); // Tells ESP32 which pin to send Servo command to
  myServo.write(0);      // Start in "Locked" position (90 is unlocked)
  currentLockState = STATE_LOCKED;

  setupLora(); // Initialize com from LoraCom.h
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
      } else {
      Serial.println("Authorized: Locking...");
      for (int pos = 90; pos <= 0; pos -= 1) { 
        myServo.write(pos);              
        delay(15);
      }
      currentLockState = STATE_LOCKED;
      send_state_update(STATE_LOCKED);
    }
  }
  else {
        Serial.println("Access Denied.");
  }

  rfid.PICC_HaltA();      // Stop reading - otherwise reader will keep reading same card and create jitter
  rfid.PCD_StopCrypto1(); // Clears the reader's internal buffer to be ready for the next card
}