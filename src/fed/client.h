#ifndef FED_CLIENT_H
#define FED_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <openssl/ssl.h>
#include <openssl/err.h>
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
    std::string ca_cert_file; // CA证书文件路径
    bool use_tls;             // 是否使用TLS
    
    ClientConfig() : 
        server_address("127.0.0.1"), 
        server_port(8080), 
        org_id("org1"),
        ca_cert_file("ca.pem"),
        use_tls(true) {}
};

class FedClient {
public:
    FedClient(const ClientConfig& config);
    bool init();
    bool process_local_data();
    bool perform_local_deduplication();
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
    
    // TLS相关
    SSL_CTX* ssl_ctx;
    SSL* ssl;
    
    std::string generate_random_id();
    std::vector<unsigned char> encrypt_signature(const std::vector<uint32_t>& signature);
    bool communicate_with_server(const std::string& message, std::string& response);
    
    // TLS相关方法
    bool init_tls();
    void cleanup_tls();
    bool establish_ssl_connection(int socket_fd);
    int ssl_read(char* buffer, int buffer_size);
    int ssl_write(const char* data, int data_size);
};

#endif // FED_CLIENT_H