#ifndef FED_CUDA_H
#define FED_CUDA_H

#include <vector>
#include <string>
#include <cstdint>

struct ShingleHashResult {
    std::vector<uint32_t> signature;
    int bucket_id;
};

class FedCuda {
public:
    FedCuda(int num_hash, int shingle_len, int num_bands, int seed);
    ~FedCuda();
    
    bool init();
    void cleanup();
    
    ShingleHashResult compute_signature(const std::string& text);
    std::vector<ShingleHashResult> compute_batch_signatures(const std::vector<std::string>& texts);
    
    int get_bucket_id(const std::vector<uint32_t>& signature);
    
private:
    int num_hash_;
    int shingle_len_;
    int num_bands_;
    int seed_;
    int rows_per_band_;
    
    unsigned int* d_p_;
    unsigned int* d_q_;
    unsigned int* d_r_;
    
    bool initialized_;
    
    void init_hash_parameters();
    void generate_hash_parameters();
};

#endif // FED_CUDA_H