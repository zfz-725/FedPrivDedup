#include "aes_crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <random>

AesCrypto::AesCrypto() {
}

AesCrypto::~AesCrypto() {
}

std::string AesCrypto::generate_key_256() {
    unsigned char key[32];
    if (RAND_bytes(key, sizeof(key)) != 1) {
        throw std::runtime_error("Failed to generate random key");
    }
    return std::string(reinterpret_cast<char*>(key), 32);
}

std::string AesCrypto::generate_iv() {
    unsigned char iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }
    return std::string(reinterpret_cast<char*>(iv), 16);
}

std::vector<unsigned char> AesCrypto::encrypt_256_cbc(
    const std::vector<unsigned char>& plaintext,
    const std::string& key,
    const std::string& iv
) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    if (key.length() != 32) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Key must be 32 bytes for AES-256");
    }
    
    if (iv.length() != 16) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("IV must be 16 bytes for AES");
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                          reinterpret_cast<const unsigned char*>(key.c_str()),
                          reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }
    
    std::vector<unsigned char> ciphertext(plaintext.size() + 16);
    int len;
    int ciphertext_len;
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to encrypt data");
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;
    
    ciphertext.resize(ciphertext_len);
    EVP_CIPHER_CTX_free(ctx);
    
    return ciphertext;
}

std::vector<unsigned char> AesCrypto::decrypt_256_cbc(
    const std::vector<unsigned char>& ciphertext,
    const std::string& key,
    const std::string& iv
) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    if (key.length() != 32) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Key must be 32 bytes for AES-256");
    }
    
    if (iv.length() != 16) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("IV must be 16 bytes for AES");
    }
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                          reinterpret_cast<const unsigned char*>(key.c_str()),
                          reinterpret_cast<const unsigned char*>(iv.c_str())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }
    
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len;
    int plaintext_len;
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                          ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to decrypt data");
    }
    plaintext_len = len;
    
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize decryption");
    }
    plaintext_len += len;
    
    plaintext.resize(plaintext_len);
    EVP_CIPHER_CTX_free(ctx);
    
    return plaintext;
}

std::string AesCrypto::encrypt_string_256_cbc(
    const std::string& plaintext,
    const std::string& key,
    const std::string& iv
) {
    std::vector<unsigned char> plaintext_vec(plaintext.begin(), plaintext.end());
    std::vector<unsigned char> ciphertext = encrypt_256_cbc(plaintext_vec, key, iv);
    return bytes_to_hex(ciphertext);
}

std::string AesCrypto::decrypt_string_256_cbc(
    const std::string& ciphertext_hex,
    const std::string& key,
    const std::string& iv
) {
    std::vector<unsigned char> ciphertext = hex_to_bytes(ciphertext_hex);
    std::vector<unsigned char> plaintext = decrypt_256_cbc(ciphertext, key, iv);
    return std::string(plaintext.begin(), plaintext.end());
}

std::string AesCrypto::bytes_to_hex(const std::vector<unsigned char>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<unsigned char> AesCrypto::hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_string = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoul(byte_string, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}