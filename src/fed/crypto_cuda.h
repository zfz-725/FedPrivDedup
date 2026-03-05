#ifndef CRYPTO_CUDA_H
#define CRYPTO_CUDA_H

#include <vector>
#include <cstdint>

// GPU加速的相似度计算
std::vector<std::pair<int, int>> gpu_calculate_similarity(
    const std::vector<std::vector<uint32_t>>& signatures,
    double threshold
);

// 检查CUDA是否可用
bool cuda_available();

#endif // CRYPTO_CUDA_H
