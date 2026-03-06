#ifndef DECRYPT_CUDA_H
#define DECRYPT_CUDA_H

#include <vector>
#include <string>
#include <cstdint>

std::vector<std::vector<uint32_t>> gpu_decrypt_signatures(
    const std::vector<std::vector<unsigned char>>& encrypted_signatures,
    const std::string& key
);

#endif // DECRYPT_CUDA_H