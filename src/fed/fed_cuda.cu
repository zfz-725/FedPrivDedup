#include "fed_cuda.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstring>
#include <algorithm>
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

__global__ void shingle_hash_kernel(
    const char* text,
    int text_len,
    const unsigned int* p,
    const unsigned int* q,
    const unsigned int* r,
    uint32_t* minhash_signature,
    int num_hash,
    int shingle_len
) {
    int hash_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (hash_idx >= num_hash) return;
    
    uint32_t min_hash = UINT32_MAX;
    
    for (int i = 0; i <= text_len - shingle_len; i++) {
        unsigned int hash = r[hash_idx];
        for (int j = 0; j < shingle_len; j++) {
            unsigned char c = static_cast<unsigned char>(text[i + j]);
            hash = (hash * p[hash_idx] + c) % q[hash_idx];
        }
        
        if (hash < min_hash) {
            min_hash = hash;
        }
    }
    
    minhash_signature[hash_idx] = min_hash;
}

__global__ void batch_shingle_hash_kernel(
    const char* texts,
    const int* text_offsets,
    const int* text_lengths,
    int num_texts,
    const unsigned int* p,
    const unsigned int* q,
    const unsigned int* r,
    uint32_t* signatures,
    int num_hash,
    int shingle_len
) {
    int text_idx = blockIdx.x;
    int hash_idx = threadIdx.x;
    
    if (text_idx >= num_texts || hash_idx >= num_hash) return;
    
    int text_offset = text_offsets[text_idx];
    int text_len = text_lengths[text_idx];
    const char* text = texts + text_offset;
    
    uint32_t* signature = signatures + text_idx * num_hash;
    
    uint32_t min_hash = UINT32_MAX;
    
    for (int i = 0; i <= text_len - shingle_len; i++) {
        unsigned int hash = r[hash_idx];
        for (int j = 0; j < shingle_len; j++) {
            unsigned char c = static_cast<unsigned char>(text[i + j]);
            hash = (hash * p[hash_idx] + c) % q[hash_idx];
        }
        
        if (hash < min_hash) {
            min_hash = hash;
        }
    }
    
    signature[hash_idx] = min_hash;
}

FedCuda::FedCuda(int num_hash, int shingle_len, int num_bands, int seed)
    : num_hash_(num_hash), shingle_len_(shingle_len), num_bands_(num_bands), seed_(seed),
      d_p_(nullptr), d_q_(nullptr), d_r_(nullptr), initialized_(false) {
    rows_per_band_ = num_hash / num_bands;
}

FedCuda::~FedCuda() {
    cleanup();
}

bool FedCuda::init() {
    if (initialized_) return true;
    
    generate_hash_parameters();
    
    unsigned int* h_p = new unsigned int[num_hash_];
    unsigned int* h_q = new unsigned int[num_hash_];
    unsigned int* h_r = new unsigned int[num_hash_];
    
    for (int i = 0; i < num_hash_; i++) {
        h_q[i] = 4294967;
        h_p[i] = 257 + i;
        h_r[i] = h_q[i] - 1;
        for (int j = 0; j < shingle_len_; j++) {
            h_r[i] = (h_r[i] * h_p[i]) % h_q[i];
        }
    }
    
    CUDA_CHECK(cudaMalloc(&d_p_, num_hash_ * sizeof(unsigned int)));
    CUDA_CHECK(cudaMalloc(&d_q_, num_hash_ * sizeof(unsigned int)));
    CUDA_CHECK(cudaMalloc(&d_r_, num_hash_ * sizeof(unsigned int)));
    
    CUDA_CHECK(cudaMemcpy(d_p_, h_p, num_hash_ * sizeof(unsigned int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_q_, h_q, num_hash_ * sizeof(unsigned int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_r_, h_r, num_hash_ * sizeof(unsigned int), cudaMemcpyHostToDevice));
    
    delete[] h_p;
    delete[] h_q;
    delete[] h_r;
    
    initialized_ = true;
    return true;
}

void FedCuda::cleanup() {
    if (d_p_) cudaFree(d_p_);
    if (d_q_) cudaFree(d_q_);
    if (d_r_) cudaFree(d_r_);
    
    d_p_ = nullptr;
    d_q_ = nullptr;
    d_r_ = nullptr;
    
    initialized_ = false;
}

void FedCuda::generate_hash_parameters() {
}

void FedCuda::init_hash_parameters() {
}

ShingleHashResult FedCuda::compute_signature(const std::string& text) {
    ShingleHashResult result;
    
    if (!initialized_) {
        std::cerr << "FedCuda not initialized" << std::endl;
        return result;
    }
    
    char* d_text;
    uint32_t* d_signature;
    
    int text_len = text.length();
    
    CUDA_CHECK(cudaMalloc(&d_text, text_len * sizeof(char)));
    CUDA_CHECK(cudaMalloc(&d_signature, num_hash_ * sizeof(uint32_t)));
    
    CUDA_CHECK(cudaMemcpy(d_text, text.c_str(), text_len * sizeof(char), cudaMemcpyHostToDevice));
    
    int threads_per_block = 256;
    int blocks = (num_hash_ + threads_per_block - 1) / threads_per_block;
    
    shingle_hash_kernel<<<blocks, threads_per_block>>>(
        d_text, text_len, d_p_, d_q_, d_r_, d_signature, num_hash_, shingle_len_
    );
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    std::vector<uint32_t> h_signature(num_hash_);
    CUDA_CHECK(cudaMemcpy(h_signature.data(), d_signature, num_hash_ * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    
    result.signature = h_signature;
    result.bucket_id = get_bucket_id(h_signature);
    
    cudaFree(d_text);
    cudaFree(d_signature);
    
    return result;
}

std::vector<ShingleHashResult> FedCuda::compute_batch_signatures(const std::vector<std::string>& texts) {
    std::vector<ShingleHashResult> results;
    
    if (!initialized_) {
        std::cerr << "FedCuda not initialized" << std::endl;
        return results;
    }
    
    if (texts.empty()) return results;
    
    int num_texts = texts.size();
    
    std::vector<int> text_offsets(num_texts);
    std::vector<int> text_lengths(num_texts);
    std::string concatenated_text;
    
    int offset = 0;
    for (int i = 0; i < num_texts; i++) {
        text_offsets[i] = offset;
        text_lengths[i] = texts[i].length();
        concatenated_text += texts[i];
        offset += text_lengths[i];
    }
    
    char* d_texts;
    int* d_text_offsets;
    int* d_text_lengths;
    uint32_t* d_signatures;
    
    CUDA_CHECK(cudaMalloc(&d_texts, concatenated_text.length() * sizeof(char)));
    CUDA_CHECK(cudaMalloc(&d_text_offsets, num_texts * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_text_lengths, num_texts * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_signatures, num_texts * num_hash_ * sizeof(uint32_t)));
    
    CUDA_CHECK(cudaMemcpy(d_texts, concatenated_text.c_str(), concatenated_text.length() * sizeof(char), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_text_offsets, text_offsets.data(), num_texts * sizeof(int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_text_lengths, text_lengths.data(), num_texts * sizeof(int), cudaMemcpyHostToDevice));
    
    dim3 grid(num_texts);
    dim3 block(num_hash_);
    
    batch_shingle_hash_kernel<<<grid, block>>>(
        d_texts, d_text_offsets, d_text_lengths, num_texts,
        d_p_, d_q_, d_r_, d_signatures, num_hash_, shingle_len_
    );
    
    CUDA_CHECK(cudaDeviceSynchronize());
    
    std::vector<uint32_t> h_signatures(num_texts * num_hash_);
    CUDA_CHECK(cudaMemcpy(h_signatures.data(), d_signatures, num_texts * num_hash_ * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    
    for (int i = 0; i < num_texts; i++) {
        ShingleHashResult result;
        result.signature.assign(h_signatures.begin() + i * num_hash_, h_signatures.begin() + (i + 1) * num_hash_);
        result.bucket_id = get_bucket_id(result.signature);
        results.push_back(result);
    }
    
    cudaFree(d_texts);
    cudaFree(d_text_offsets);
    cudaFree(d_text_lengths);
    cudaFree(d_signatures);
    
    return results;
}

int FedCuda::get_bucket_id(const std::vector<uint32_t>& signature) {
    int bucket_id = 0;
    
    for (int b = 0; b < num_bands_; b++) {
        unsigned int sum = 0;
        for (int k = b * rows_per_band_; k < (b + 1) * rows_per_band_; k++) {
            sum += signature[k];
        }
        bucket_id = sum % num_bands_;
    }
    
    return bucket_id;
}