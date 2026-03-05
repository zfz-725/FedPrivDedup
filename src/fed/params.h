#ifndef FED_PARAMS_H
#define FED_PARAMS_H

#include <string>

struct FedParams {
    int num_hash;          // 哈希函数数量
    int shingle_len;       // n-gram长度
    int bucket;            // 桶数
    int seed;              // 随机种子
    double threshold;      // 相似度阈值
    int num_file;          // 文件数量
    
    // 默认构造函数
    FedParams() : 
        num_hash(128), 
        shingle_len(5), 
        bucket(16), 
        seed(777984), 
        threshold(0.8), 
        num_file(1000) {}
    
    // 构造函数
    FedParams(int nh, int sl, int b, int s, double t, int nf) : 
        num_hash(nh), 
        shingle_len(sl), 
        bucket(b), 
        seed(s), 
        threshold(t), 
        num_file(nf) {}
};

#endif // FED_PARAMS_H