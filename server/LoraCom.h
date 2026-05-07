#pragma once

#include "Arduino.h"
#include "HardwareSerial.h"

extern HardwareSerial loraSerial;

void setupLora();
void sendMessage(unsigned char* fullPacket, size_t packetLength);
void rearmReceive();
