#ifndef FED_SERVER_H
#define FED_SERVER_H

#include <string>
#include <vector>
#include <map>
#include "params.h"

struct EncryptedCandidate {
    std::string client_id;
    std::string doc_id;
    std::vector<unsigned char> encrypted_signature;
    int bucket_id;
    std::string org_id;
};

struct ServerConfig {
    std::string address;
    int port;
    int max_clients;
    FedParams params;
    
    ServerConfig() : 
        address("127.0.0.1"), 
        port(8080), 
        max_clients(10) {}
};

class FedServer {
public:
    FedServer(const ServerConfig& config);
    bool init();
    bool start();
    void handle_client_request(const std::string& request, std::string& response);
    bool perform_global_similarity();
    bool distribute_results();
    
    // 新增：发送全局参数
    std::string get_global_params();
    
private:
    ServerConfig config;
    std::vector<EncryptedCandidate> candidates;
    std::map<int, std::vector<EncryptedCandidate>> bucket_candidates;
    std::map<std::string, std::vector<std::string>> duplicate_docs;
    std::map<std::string, std::string> client_keys;
    
    // 用于处理大数据传输
    size_t pending_data_size;
    std::string pending_data_buffer;
    
   // 工具函数
    std::vector<uint32_t> decrypt_signature(const std::vector<unsigned char>& encrypted, const std::string& client_id);
    double calculate_similarity(const std::vector<uint32_t>& sig1, const std::vector<uint32_t>& sig2);
    int calculate_dynamic_buckets(int num_files); // 动态计算桶数
    std::string generate_client_key();
};

#endif // FED_SERVER_H