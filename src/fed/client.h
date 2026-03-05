#ifndef FED_CLIENT_H
#define FED_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include "params.h"

struct DocumentInfo {
    std::string original_id;
    std::string random_id;
    std::vector<uint32_t> signature;
    int bucket_id;
};

struct ClientConfig {
    std::string client_id;
    std::string server_address;
    int server_port;
    std::string data_dir;
    std::string output_dir;
    std::string org_id;
    FedParams local_params;
    
    ClientConfig() : 
        server_address("127.0.0.1"), 
        server_port(8080), 
        org_id("org1") {}
};

class FedClient {
public:
    FedClient(const ClientConfig& config);
    bool init();
    bool process_local_data();
    bool send_encrypted_candidates();
    bool trigger_global_calculation();
    bool receive_and_process_results();
    bool run();
    
    // 新增：获取全局参数
    bool get_global_params();
    
private:
    ClientConfig config;
    std::map<std::string, std::string> id_mapping;
    std::vector<DocumentInfo> local_candidates;
    std::string server_key;
    
    std::string generate_random_id();
    std::vector<unsigned char> encrypt_signature(const std::vector<uint32_t>& signature);
    bool communicate_with_server(const std::string& message, std::string& response);
};

#endif // FED_CLIENT_H