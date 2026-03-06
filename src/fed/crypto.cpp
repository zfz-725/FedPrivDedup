#include "crypto.h"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace std;

std::string CryptoUtil::generate_key(int length) {
    if (length != 32) {
        cerr << "[Crypto] Warning: AES-256 requires 32-byte key, using 32 bytes" << endl;
    }
    return AesCrypto::generate_key_256();
}

std::vector<unsigned char> CryptoUtil::encrypt(const std::vector<unsigned char>& data, const std::string& key) {
    std::string iv = generate_iv();
    return AesCrypto::encrypt_256_cbc(data, key, iv);
}

std::vector<unsigned char> CryptoUtil::decrypt(const std::vector<unsigned char>& data, const std::string& key) {
    std::string iv = std::string(16, '\0');
    return AesCrypto::decrypt_256_cbc(data, key, iv);
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
    std::string iv = generate_iv();
    return AesCrypto::encrypt_string_256_cbc(data, key, iv);
}

std::string CryptoUtil::decrypt_string(const std::string& data, const std::string& key) {
    std::string iv = std::string(16, '\0');
    return AesCrypto::decrypt_string_256_cbc(data, key, iv);
}

std::string CryptoUtil::generate_iv() {
    return AesCrypto::generate_iv();
}