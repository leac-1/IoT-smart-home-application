#include <stdint.h>
#include <string.h>
// This library proves the CCM mode of operation for block ciphers.
// We don't need to reinvent the wheel, so i'll use the mbedtls library for the calculations of the MIC and the en- and decryption of the payload.
#include "mbedtls/ccm.h" 

// This function is based on RFC 3610. It uses AES-128 in CCM mode to encrypt our payload and calculate a MIC for authentication of the entire packet.

// Shared secret key for AES-128 encryption (16 bytes)
static const unsigned char AES_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// We choose M = 4 as the length of the authentication tag (MIC) giving 2^32 attempts to forge a valid MIC
// and L = 2 as the length of the payload yielding 2^(8*L)=2^16 = 65536 bytes.
// Our payload is max 6 bytes for curtain state, so L=2 is sufficient
// This gives a Nonce length of = 15-L = 13 bytes
static void build_nonce(unsigned char*nonce, unsigned char src_id, uint16_t counter){
    memset(nonce, 0, 13); // Clear the memory position for the nonce
    nonce[0] = src_id; // set the first byte to the source ID
    nonce[1] = (counter >> 8) & 0xFF; // set the second byte to the high byte of the counter (anding with 0xFF to ensure we only get 1 byte)
    nonce[2] = counter & 0xFF; // set the third byte to the low byte of the counter
    // The RFC specifies that the remaining bytes should be set to 0, but we've already done that with memset
}

int encrypt_and_mic(const unsigned char* header, const unsigned char* plaintext, size_t plaintext_len, unsigned char* enc_out, unsigned char* mic_out) {
    unsigned char src_id = header[0];
    uint16_t counter = (header[3] << 8) | header[4]; // [3] is high byte and [4] is low byte. We left-shift the high-byte by 8 bits and then bitwise OR with the low byte to get the actual value

    const unsigned char* adata = header; // additional data IS the header
    size_t adata_len = 5; // fixed size of header
    unsigned char nonce[13];
    build_nonce(nonce,src_id,counter);

    mbedtls_ccm_context ctx; // Declare the CCM context
    mbedtls_ccm_init(&ctx); // Initialize the CCM context
    int ret = mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES, AES_KEY, 128); // Set the key for AES-128 encryption
    if (ret !=0) {
        mbedtls_ccm_free(&ctx); // Free the CCM context to avoid memory leaks
        return -1; // error
    }

    ret = mbedtls_ccm_encrypt_and_tag(
        &ctx,
        plaintext_len,
        nonce, 13,
        adata, adata_len,
        plaintext,
        enc_out,
        mic_out,4
    );
    mbedtls_ccm_free(&ctx);
    return (ret ==0) ? 0 : -1; // return 0 on success, -1 on error
}

// The recieving end will do the opposite of the encryption, still using the library to verify the MIC and decrypt the payload.
int decrypt_and_verify(const unsigned char* header, const unsigned char* ciphertext, size_t ciphertext_len, const unsigned char* mic, unsigned char* plaintext_out) {
    unsigned char src_id = header[0];
    uint16_t counter = (header[3] << 8) | header[4];

    unsigned char nonce[13];
    build_nonce(nonce,src_id,counter);

    const unsigned char* adata = header;
    size_t adata_len = 5;

    mbedtls_ccm_context ctx;
    mbedtls_ccm_init(&ctx);
    int ret = mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES, AES_KEY, 128);
    if (ret != 0) {
        mbedtls_ccm_free(&ctx);
        return -1; // error
    }

    ret = mbedtls_ccm_auth_decrypt(
        &ctx,
        ciphertext_len,
        nonce, 13,
        adata, adata_len,
        ciphertext,
        plaintext_out,
        mic,4
    );
    mbedtls_ccm_free(&ctx);
    return (ret == 0) ? 0 : -1; // return 0 on success, -1 on error 
}

int decrypt_packet(const unsigned char* raw, size_t raw_len, unsigned char* plaintext_out) {
    // Minimum: header(5) + at least 1 byte payload + MIC(4) + CRC(1) = 11 bytes
    if (raw_len < 11) return -1;

    const size_t header_len = 5;
    const size_t mic_len = 4;
    const size_t crc_len = 1;
    size_t payload_len = raw_len - header_len - mic_len - crc_len;

    const unsigned char* header     = raw;
    const unsigned char* ciphertext = raw + header_len;
    const unsigned char* mic        = raw + header_len + payload_len;

    return decrypt_and_verify(header, ciphertext, payload_len, mic, plaintext_out);
}
