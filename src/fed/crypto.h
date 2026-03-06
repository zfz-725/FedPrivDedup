#ifndef FED_CRYPTO_H
#define FED_CRYPTO_H

#include <string>
#include <vector>
#include "aes_crypto.h"

class CryptoUtil {
public:
    // 生成随机密钥（AES-256）
    static std::string generate_key(int length = 32);
    
    // AES-256-CBC加密
    static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data, const std::string& key);
    
    // AES-256-CBC解密
    static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data, const std::string& key);
    
    // 生成无语义随机ID
    static std::string generate_random_id();
    
    // 加密字符串
    static std::string encrypt_string(const std::string& data, const std::string& key);
    
    // 解密字符串
    static std::string decrypt_string(const std::string& data, const std::string& key);
    
    // 生成IV
    static std::string generate_iv();
};

#endif // FED_CRYPTO_H