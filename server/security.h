#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include <stddef.h>

// =========================================================
// HOW TO USE security.cpp
// =========================================================
//
// This file provides two functions:
//
// 1. encrypt_and_mic(header, plaintext, plaintext_len, enc_out, mic_out)
//    - Call this on the SENDER side (node) before transmitting
//    - Takes your plaintext payload and header
//    - Writes encrypted payload into enc_out
//    - Writes 4-byte MIC into mic_out
//    - Returns 0 on success, -1 on failure
//
// 2. decrypt_and_verify(header, ciphertext, ciphertext_len, mic, plaintext_out)
//    - Call this on the RECEIVER side (gateway) after receiving
//    - Takes encrypted payload, header and MIC
//    - Verifies MIC — if it fails, the packet was tampered with
//    - Writes decrypted payload into plaintext_out
//    - Returns 0 if MIC is valid, -1 if packet is invalid
//
// IMPORTANT: The header is NOT encrypted, only MIC-protected.
// The header format is 5 bytes: [src, dst, type, counter_hi, counter_lo]
// No preamble — our LoRa module handles synchronisation (i think/hope).
//
// In build_packet, replace the dummy MIC() call with:
//
//    encrypt_and_mic(header, data, data_len,
//                    packet + header_len,
//                    packet + header_len + data_len);
//
// Note: data must be the plaintext BEFORE encryption.
// After this call, the payload in the packet is encrypted.
//
// In verifyPacketMICAndCRC, replace the dummy MIC check with:
//
//    unsigned char decrypted[MAX_PAYLOAD];
//    int ret = decrypt_and_verify(packet,
//                    packet + header_len,
//                    data_len,
//                    packet + header_len + data_len,
//                    decrypted);
//    if (ret != 0) return false;  // MIC failed — reject packet
//
// OBS!!!!!Also: change counter from unsigned char to uint16_t everywhere.
// unsigned char only goes to 255 — we need 0-65535.
// =========================================================

int encrypt_and_mic(const unsigned char* header,
                    const unsigned char* plaintext,
                    size_t plaintext_len,
                    unsigned char* enc_out,
                    unsigned char* mic_out);

int decrypt_and_verify(const unsigned char* header,
                       const unsigned char* ciphertext,
                       size_t ciphertext_len,
                       const unsigned char* mic,
                       unsigned char* plaintext_out);

#endif