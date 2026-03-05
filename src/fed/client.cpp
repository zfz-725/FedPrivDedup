#include "client.h"
#include "crypto.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

using namespace std;

FedClient::FedClient(const ClientConfig& config) : config(config) {
}

bool FedClient::init() {
    cout << "[Client] Initializing client..." << endl;
    // 连接服务器获取全局参数
    if (!get_global_params()) {
        cout << "[Client] Failed to get global parameters" << endl;
        return false;
    }
    return true;
}

bool FedClient::get_global_params() {
    cout << "[Client] Getting global parameters from server..." << endl;
    
    // 发送连接请求
    std::string message = "HELLO " + config.client_id;
    std::string response;
    if (!communicate_with_server(message, response)) {
        cout << "[Client] Failed to communicate with server" << endl;
        return false;
    }
    
    // 解析响应
    std::istringstream iss(response);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "PARAMS") {
        // 解析全局参数
        iss >> config.local_params.num_hash
            >> config.local_params.shingle_len
            >> config.local_params.bucket
            >> config.local_params.seed
            >> config.local_params.threshold
            >> config.local_params.num_file;
        
        // 解析密钥
        std::string key_cmd;
        iss >> key_cmd;
        if (key_cmd == "KEY") {
            iss >> server_key;
            cout << "[Client] Received global parameters and key" << endl;
            return true;
        }
    }
    
    return false;
}

bool FedClient::process_local_data() {
    cout << "[Client] Processing local data..." << endl;
    // 模拟处理本地数据
    for (int i = 0; i < 10; ++i) {
        DocumentInfo doc;
        doc.original_id = "doc" + to_string(i);
        doc.random_id = generate_random_id();
        
        // 生成模拟签名
        for (int j = 0; j < config.local_params.num_hash; ++j) {
            doc.signature.push_back(j + i);
        }
        
        doc.bucket_id = i % config.local_params.bucket;
        local_candidates.push_back(doc);
        id_mapping[doc.random_id] = doc.original_id;
    }
    return true;
}

std::vector<unsigned char> FedClient::encrypt_signature(const std::vector<uint32_t>& signature) {
    // 转换为字节数组
    std::vector<unsigned char> data;
    for (uint32_t val : signature) {
        unsigned char bytes[4];
        memcpy(bytes, &val, 4);
        data.insert(data.end(), bytes, bytes + 4);
    }
    
    // 使用AES加密
    return CryptoUtil::encrypt(data, server_key);
}

bool FedClient::send_encrypted_candidates() {
    cout << "[Client] Sending encrypted candidates..." << endl;
    // 简化实现，实际应发送加密的候选集
    std::string message = "CANDIDATE test";
    std::string response;
    return communicate_with_server(message, response);
}

bool FedClient::receive_and_process_results() {
    cout << "[Client] Receiving and processing results..." << endl;
    // 简化实现，实际应接收并处理结果
    std::string message = "GET_RESULTS";
    std::string response;
    return communicate_with_server(message, response);
}

bool FedClient::run() {
    if (!init()) return false;
    if (!process_local_data()) return false;
    if (!send_encrypted_candidates()) return false;
    if (!receive_and_process_results()) return false;
    return true;
}

std::string FedClient::generate_random_id() {
    return CryptoUtil::generate_random_id();
}

bool FedClient::communicate_with_server(const std::string& message, std::string& response) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "[Client] Socket creation error" << endl;
        return false;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.server_port);
    
    if (inet_pton(AF_INET, config.server_address.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "[Client] Invalid address" << endl;
        return false;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "[Client] Connection failed" << endl;
        return false;
    }
    
    send(sock, message.c_str(), message.length(), 0);
    
    char buffer[1024] = {0};
    int valread = read(sock, buffer, 1024);
    response = string(buffer, valread);
    
    close(sock);
    return true;
}