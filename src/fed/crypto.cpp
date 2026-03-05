#include "crypto.h"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace std;

// 简化的AES实现（用于演示）
void CryptoUtil::aes_encrypt_block(unsigned char* in, unsigned char* out, const unsigned char* key) {
    // 简化的AES加密实现
    // 实际项目中应使用openssl等库
    for (int i = 0; i < 16; ++i) {
        out[i] = in[i] ^ key[i % 32];
    }
}

void CryptoUtil::aes_decrypt_block(unsigned char* in, unsigned char* out, const unsigned char* key) {
    // 简化的AES解密实现
    for (int i = 0; i < 16; ++i) {
        out[i] = in[i] ^ key[i % 32];
    }
}

std::string CryptoUtil::generate_key(int length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    std::string key;
    for (int i = 0; i < length; ++i) {
        key += static_cast<char>(dis(gen));
    }
    return key;
}

std::vector<unsigned char> CryptoUtil::encrypt(const std::vector<unsigned char>& data, const std::string& key) {
    std::vector<unsigned char> result;
    unsigned char in[16] = {0};
    unsigned char out[16] = {0};
    
    for (size_t i = 0; i < data.size(); i += 16) {
        size_t block_size = std::min(16UL, data.size() - i);
        memset(in, 0, 16);
        memcpy(in, &data[i], block_size);
        
        aes_encrypt_block(in, out, reinterpret_cast<const unsigned char*>(key.c_str()));
        result.insert(result.end(), out, out + 16);
    }
    
    return result;
}

std::vector<unsigned char> CryptoUtil::decrypt(const std::vector<unsigned char>& data, const std::string& key) {
    std::vector<unsigned char> result;
    unsigned char in[16] = {0};
    unsigned char out[16] = {0};
    
    for (size_t i = 0; i < data.size(); i += 16) {
        size_t block_size = std::min(16UL, data.size() - i);
        memset(in, 0, 16);
        memcpy(in, &data[i], block_size);
        
        aes_decrypt_block(in, out, reinterpret_cast<const unsigned char*>(key.c_str()));
        result.insert(result.end(), out, out + 16);
    }
    
    return result;
}

std::string CryptoUtil::generate_random_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 16; ++i) {
        ss << std::setw(1) << std::setfill('0') << dis(gen);
        if (i < 15 && i % 4 == 3) ss << "-";
    }
    return ss.str();
}

std::string CryptoUtil::encrypt_string(const std::string& data, const std::string& key) {
    std::vector<unsigned char> data_vec(data.begin(), data.end());
    std::vector<unsigned char> encrypted = encrypt(data_vec, key);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : encrypted) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

std::string CryptoUtil::decrypt_string(const std::string& data, const std::string& key) {
    std::vector<unsigned char> encrypted;
    for (size_t i = 0; i < data.size(); i += 2) {
        std::string byte_str = data.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16));
        encrypted.push_back(byte);
    }
    
    std::vector<unsigned char> decrypted = decrypt(encrypted, key);
    return std::string(decrypted.begin(), decrypted.end());
}