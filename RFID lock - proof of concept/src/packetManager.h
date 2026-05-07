#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern unsigned char* source_address;

unsigned char* build_packet(unsigned char type, unsigned char* data, size_t data_len, const unsigned char* des, uint16_t counter, size_t* out_packet_len);
bool verifyPacketMICAndCRC(unsigned char* packet, size_t packetLength);
unsigned char* buildBeaconPayload(uint8_t SlotDuration, uint32_t cycleTime, uint8_t CurrentSlotCount);
unsigned char CRC8(const unsigned char* data, size_t len);