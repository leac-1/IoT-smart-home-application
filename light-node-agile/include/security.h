#pragma once
#include <stddef.h>

int encrypt_and_mic(const unsigned char* header, const unsigned char* plaintext, size_t plaintext_len, unsigned char* enc_out, unsigned char* mic_out);

int decrypt_and_verify(const unsigned char* header, const unsigned char* ciphertext, size_t ciphertext_len, const unsigned char* mic, unsigned char* plaintext_out);

int decrypt_packet(const unsigned char* raw, size_t raw_len, unsigned char* plaintext_out);
