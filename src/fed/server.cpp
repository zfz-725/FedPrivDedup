#include "server.h"
#include "crypto.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sstream>

using namespace std;

FedServer::FedServer(const ServerConfig& config) : config(config) {
}

bool FedServer::init() {
    cout << "[Server] Initializing server..." << endl;
    return true;
}

std::string FedServer::generate_client_key() {
    return CryptoUtil::generate_key();
}

std::string FedServer::get_global_params() {
    std::stringstream ss;
    ss << "PARAMS " 
       << config.params.num_hash << " " 
       << config.params.shingle_len << " " 
       << config.params.bucket << " " 
       << config.params.seed << " " 
       << config.params.threshold << " " 
       << config.params.num_file;
    return ss.str();
}

void FedServer::handle_client_request(const std::string& request, std::string& response) {
    if (request.substr(0, 6) == "HELLO ") {
        // 客户端连接请求
        std::string client_id = request.substr(6);
        std::string key = generate_client_key();
        client_keys[client_id] = key;
        
        // 发送全局参数和密钥
        std::string params = get_global_params();
        response = params + " KEY " + key;
        cout << "[Server] New client connected: " << client_id << endl;
    } else if (request.substr(0, 8) == "CANDIDATE") {
        // 处理候选集
        // 简化处理，实际应解析具体的候选集数据
        candidates.push_back({"test", "doc1", {}, 0, "org1"});
        response = "ACK";
    } else if (request == "CALCULATE") {
        // 执行全局相似度计算
        perform_global_similarity();
        response = "DONE";
    } else if (request == "GET_RESULTS") {
        // 分发结果
        distribute_results();
        response = "RESULTS_SENT";
    } else {
        response = "UNKNOWN_COMMAND";
    }
}

bool FedServer::start() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return false;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return false;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(config.address.c_str());
    address.sin_port = htons(config.port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return false;
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return false;
    }
    
    cout << "[Server] Server started on " << config.address << ":" << config.port << endl;
    
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        thread([this, new_socket]() {
            char buffer[1024] = {0};
            while (true) {
                int valread = read(new_socket, buffer, 1024);
                if (valread <= 0) break;
                
                std::string request(buffer, valread);
                std::string response;
                handle_client_request(request, response);
                
                send(new_socket, response.c_str(), response.length(), 0);
                memset(buffer, 0, 1024);
            }
            close(new_socket);
        }).detach();
    }
    
    return true;
}

std::vector<uint32_t> FedServer::decrypt_signature(const std::vector<unsigned char>& encrypted, const std::string& client_id) {
    // 简化实现，实际应使用客户端密钥解密
    std::vector<uint32_t> signature;
    for (size_t i = 0; i < 128; ++i) {
        signature.push_back(i);
    }
    return signature;
}

double FedServer::calculate_similarity(const std::vector<uint32_t>& sig1, const std::vector<uint32_t>& sig2) {
    // 简化实现，实际应计算Jaccard相似度
    return 0.85;
}

bool FedServer::perform_global_similarity() {
    cout << "[Server] Performing global similarity calculation..." << endl;
    // 简化实现，实际应按桶处理
    duplicate_docs["client1"].push_back("doc1");
    duplicate_docs["client1"].push_back("doc2");
    return true;
}

bool FedServer::distribute_results() {
    cout << "[Server] Distributing results..." << endl;
    return true;
}