#pragma once
#include <stddef.h>

extern unsigned char* source_address;

unsigned char* build_header(unsigned char type, const unsigned char* des);
unsigned char* build_packet(unsigned char type, const unsigned char* data, const unsigned char* des, size_t data_len);
unsigned char* MIC(unsigned char* type, const unsigned char* des);
unsigned char CRC8(const unsigned char* data, size_t len);
void set_packet_counter(uint16_t value);
uint16_t get_packet_counter();