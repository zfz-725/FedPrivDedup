#include "crypto.h"
#include <iostream>
#include <vector>
#include <cstdint>

// CUDA头文件
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

// GPU矩阵化相似度对比核函数 - 计算真实Jaccard相似度
__global__ void similarity_compare_kernel(
    const uint32_t* signatures,      // 所有签名 [num_docs * num_hash]
    float* similarity_matrix,         // 相似度矩阵 [num_docs * num_docs]
    int num_docs,                    // 文档数量
    int num_hash,                    // 哈希函数数量
    float threshold                   // 相似度阈值
) {
    // 使用tiling策略
    __shared__ uint32_t tile1[32][32];  // 共享内存tile
    __shared__ uint32_t tile2[32][32];
    
    int doc_i = blockIdx.x * 32 + threadIdx.x;
    int doc_j = blockIdx.y * 32 + threadIdx.y;
    
    if (doc_i >= num_docs || doc_j >= num_docs || doc_i >= doc_j) return;
    
    int match_count = 0;
    
    // 分tile处理
    for (int tile_offset = 0; tile_offset < num_hash; tile_offset += 32) {
        // 加载tile到共享内存
        if (doc_i < num_docs && tile_offset + threadIdx.y < num_hash) {
            tile1[threadIdx.x][threadIdx.y] = signatures[doc_i * num_hash + tile_offset + threadIdx.y];
        }
        if (doc_j < num_docs && tile_offset + threadIdx.x < num_hash) {
            tile2[threadIdx.y][threadIdx.x] = signatures[doc_j * num_hash + tile_offset + threadIdx.x];
        }
        __syncthreads();
        
        // 比较tile内的元素
        #pragma unroll
        for (int k = 0; k < 32 && tile_offset + k < num_hash; k++) {
            if (tile1[threadIdx.x][k] == tile2[threadIdx.y][k]) {
                match_count++;
            }
        }
        __syncthreads();
    }
    
    // 计算Jaccard相似度
    float jaccard_similarity = static_cast<float>(match_count) / num_hash;
    
    // 写入结果矩阵
    if (doc_i < doc_j && doc_j < num_docs) {
        similarity_matrix[doc_i * num_docs + doc_j] = jaccard_similarity;
    }
}

// GPU加速的相似度计算 - 计算真实Jaccard相似度
std::vector<std::pair<int, int>> gpu_calculate_similarity(
    const std::vector<std::vector<uint32_t>>& signatures,
    double threshold
) {
    std::vector<std::pair<int, int>> duplicates;
    
    int num_docs = signatures.size();
    if (num_docs < 2) return duplicates;
    
    int num_hash = signatures[0].size();
    float threshold_float = static_cast<float>(threshold);
    
    // 准备GPU内存
    uint32_t* d_signatures;
    float* d_similarity;
    
    size_t sig_size = num_docs * num_hash * sizeof(uint32_t);
    size_t similarity_size = num_docs * num_docs * sizeof(float);
    
    cudaMalloc(&d_signatures, sig_size);
    cudaMalloc(&d_similarity, similarity_size);
    
    // 复制数据到GPU
    std::vector<uint32_t> flat_signatures;
    for (const auto& sig : signatures) {
        flat_signatures.insert(flat_signatures.end(), sig.begin(), sig.end());
    }
    
    cudaMemcpy(d_signatures, flat_signatures.data(), sig_size, cudaMemcpyHostToDevice);
    cudaMemset(d_similarity, 0, similarity_size);
    
    // 配置网格和块
    dim3 blockDim(32, 32);
    dim3 gridDim((num_docs + 31) / 32, (num_docs + 31) / 32);
    
    // 启动核函数
    similarity_compare_kernel<<<gridDim, blockDim>>>(
        d_signatures, d_similarity, num_docs, num_hash, threshold_float
    );
    
    // 等待核函数完成
    cudaDeviceSynchronize();
    
    // 复制结果回CPU
    std::vector<float> similarity_matrix(num_docs * num_docs);
    cudaMemcpy(similarity_matrix.data(), d_similarity, similarity_size, cudaMemcpyDeviceToHost);
    
    // 解析结果 - 使用阈值过滤
    for (int i = 0; i < num_docs; i++) {
        for (int j = i + 1; j < num_docs; j++) {
            float sim = similarity_matrix[i * num_docs + j];
            if (sim >= threshold_float) {
                duplicates.emplace_back(i, j);
            }
        }
    }
    
    // 释放GPU内存
    cudaFree(d_signatures);
    cudaFree(d_similarity);
    
    return duplicates;
}

// 检查CUDA是否可用
bool cuda_available() {
    int deviceCount;
    cudaGetDeviceCount(&deviceCount);
    return deviceCount > 0;
}
