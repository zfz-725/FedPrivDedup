#ifndef AES_CRYPTO_H
#define AES_CRYPTO_H

#include <string>
#include <vector>
#include <cstdint>

class AesCrypto {
public:
    AesCrypto();
    ~AesCrypto();
    
    static std::string generate_key_256();
    static std::string generate_iv();
    
    static std::vector<unsigned char> encrypt_256_cbc(
        const std::vector<unsigned char>& plaintext,
        const std::string& key,
        const std::string& iv
    );
    
    static std::vector<unsigned char> decrypt_256_cbc(
        const std::vector<unsigned char>& ciphertext,
        const std::string& key,
        const std::string& iv
    );
    
    static std::string encrypt_string_256_cbc(
        const std::string& plaintext,
        const std::string& key,
        const std::string& iv
    );
    
    static std::string decrypt_string_256_cbc(
        const std::string& ciphertext_hex,
        const std::string& key,
        const std::string& iv
    );
    
    static std::string bytes_to_hex(const std::vector<unsigned char>& data);
    static std::vector<unsigned char> hex_to_bytes(const std::string& hex);
};

#endif // AES_CRYPTO_H