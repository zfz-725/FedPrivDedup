#include "decrypt_cuda.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstring>
#include <iostream>
#include <vector>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error: " << cudaGetErrorString(err) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            exit(1); \
        } \
    } while(0)

__global__ void simple_decrypt_kernel(
    const unsigned char* encrypted_data,
    uint32_t* decrypted_signatures,
    int num_signatures,
    int signature_size,
    const unsigned char* key
) {
    int sig_idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (sig_idx >= num_signatures) return;
    
    uint32_t* signature = decrypted_signatures + sig_idx * signature_size;
    const unsigned char* encrypted = encrypted_data + sig_idx * signature_size * 4;
    
    for (int i = 0; i < signature_size; i++) {
        uint32_t val = 0;
        for (int j = 0; j < 4; j++) {
            unsigned char byte = encrypted[i * 4 + j];
            val |= (static_cast<uint32_t>(byte) << (j * 8));
        }
        
        signature[i] = val ^ key[i % 32];
    }
}

std::vector<std::vector<uint32_t>> gpu_decrypt_signatures(
    const std::vector<std::vector<unsigned char>>& encrypted_signatures,
    const std::string& key
) {
    std::vector<std::vector<uint32_t>> decrypted_signatures;
    
    if (encrypted_signatures.empty()) {
        return decrypted_signatures;
    }
    
    int num_signatures = encrypted_signatures.size();
    int signature_size = encrypted_signatures[0].size() / 4;
    
    if (key.length() < 32) {
        std::cerr << "[GPU Decrypt] Key too short" << std::endl;
        return decrypted_signatures;
    }
    
    unsigned char* d_encrypted_data;
    uint32_t* d_decrypted_signatures;
    unsigned char* d_key;
    
    size_t encrypted_size = num_signatures * signature_size * 4 * sizeof(unsigned char);
    size_t decrypted_size = num_signatures * signature_size * sizeof(uint32_t);
    size_t key_size = 32 * sizeof(unsigned char);
    
    CUDA_CHECK(cudaMalloc(&d_encrypted_data, encrypted_size));
    CUDA_CHECK(cudaMalloc(&d_decrypted_signatures, decrypted_size));
    CUDA_CHECK(cudaMalloc(&d_key, key_size));
    
    std::vector<unsigned char> flat_encrypted;
    for (const auto& sig : encrypted_signatures) {
        flat_encrypted.insert(flat_encrypted.end(), sig.begin(), sig.end());
    }
    
    CUDA_CHECK(cudaMemcpy(d_encrypted_data, flat_encrypted.data(), encrypted_size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_key, key.c_str(), key_size, cudaMemcpyHostToDevice));
    
    int threads_per_block = 256;
    int blocks = (num_signatures + threads_per_block - 1) / threads_per_block;
    
    simple_decrypt_kernel<<<blocks, threads_per_block>>>(
        d_encrypted_data, d_decrypted_signatures, num_signatures, signature_size, d_key
    );
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    std::vector<uint32_t> flat_decrypted(num_signatures * signature_size);
    CUDA_CHECK(cudaMemcpy(flat_decrypted.data(), d_decrypted_signatures, decrypted_size, cudaMemcpyDeviceToHost));
    
    for (int i = 0; i < num_signatures; i++) {
        std::vector<uint32_t> signature(
            flat_decrypted.begin() + i * signature_size,
            flat_decrypted.begin() + (i + 1) * signature_size
        );
        decrypted_signatures.push_back(signature);
    }
    
    cudaFree(d_encrypted_data);
    cudaFree(d_decrypted_signatures);
    cudaFree(d_key);
    
    return decrypted_signatures;
}