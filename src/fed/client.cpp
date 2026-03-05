#include "client.h"
#include "crypto.h"
#include "lsh.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <random>

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
    std::string message = "HELLO " + config.client_id + " " + config.org_id + "\n";
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
    
    namespace fs = std::filesystem;
    
    std::string data_dir = config.data_dir;
    std::string output_dir = config.output_dir;
    
    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }
    
    cout << "[Client] Processing files in " << data_dir << "..." << endl;
    
    std::vector<std::string> file_list;
    for (const auto& entry : fs::directory_iterator(data_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
            file_list.push_back(entry.path().string());
        }
    }
    
    if (file_list.empty()) {
        cout << "[Client] No JSONL files found in " << data_dir << endl;
        cout << "[Client] Generating sample data for testing..." << endl;
        
        int num_samples = 100;
        int num_hash = config.local_params.num_hash;
        int bucket_count = config.local_params.bucket;
        
        for (int i = 0; i < num_samples; ++i) {
            DocumentInfo doc;
            doc.original_id = config.client_id + "_doc" + std::to_string(i);
            doc.random_id = generate_random_id();
            
            std::vector<uint32_t> signature;
            
            // 前10个文档使用相同的种子，这样不同客户端会生成相同的签名用于测试跨机构去重
            unsigned int seed;
            if (i < 10) {
                seed = config.local_params.seed + i; // 使用相同的种子
            } else {
                seed = config.local_params.seed + i + 1000; // 不同的种子
            }
            
            std::mt19937 gen(seed);
            std::uniform_int_distribution<uint32_t> dis(0, 4294967);
            
            for (int j = 0; j < num_hash; ++j) {
                signature.push_back(dis(gen));
            }
            
            doc.signature = signature;
            
            int bucket_id = 0;
            int h = num_hash / bucket_count;
            for (int b = 0; b < bucket_count; ++b) {
                unsigned int sum = 0;
                for (int k = b * h; k < (b + 1) * h; ++k) {
                    sum += signature[k];
                }
                bucket_id = sum % bucket_count;
            }
            doc.bucket_id = bucket_id;
            
            local_candidates.push_back(doc);
            id_mapping[doc.random_id] = doc.original_id;
        }
        
        cout << "[Client] Generated " << local_candidates.size() << " sample documents" << endl;
        return true;
    }
    
    cout << "[Client] Found " << file_list.size() << " files to process" << endl;
    
    for (size_t file_idx = 0; file_idx < file_list.size(); ++file_idx) {
        fs::path input_path(file_list[file_idx]);
        std::string filename = input_path.stem().string();
        std::string hash_file = output_dir + "/" + filename + "_hashresult.bin";
        
        if (fs::exists(hash_file)) {
            std::ifstream hash_in(hash_file, std::ios::binary);
            if (hash_in) {
                int total_values = config.local_params.num_hash + config.local_params.bucket;
                std::vector<uint32_t> hash_data;
                hash_data.resize(total_values);
                
                int doc_idx = 0;
                while (hash_in.read(reinterpret_cast<char*>(hash_data.data()), total_values * sizeof(uint32_t))) {
                    DocumentInfo doc;
                    doc.original_id = filename + "_doc" + std::to_string(doc_idx);
                    doc.random_id = generate_random_id();
                    
                    for (int i = 0; i < config.local_params.num_hash; ++i) {
                        doc.signature.push_back(hash_data[i]);
                    }
                    
                    int bucket_id = hash_data[config.local_params.num_hash];
                    doc.bucket_id = bucket_id;
                    
                    local_candidates.push_back(doc);
                    id_mapping[doc.random_id] = doc.original_id;
                    
                    doc_idx++;
                }
                
                hash_in.close();
                cout << "[Client] Loaded " << doc_idx << " documents from " << hash_file << endl;
            }
        } else {
            cout << "[Client] Hash result file not found: " << hash_file << endl;
        }
    }
    
    if (local_candidates.empty()) {
        cout << "[Client] No candidates loaded, generating sample data..." << endl;
        int num_samples = 100;
        int num_hash = config.local_params.num_hash;
        int bucket_count = config.local_params.bucket;
        
        for (int i = 0; i < num_samples; ++i) {
            DocumentInfo doc;
            doc.original_id = config.client_id + "_doc" + std::to_string(i);
            doc.random_id = generate_random_id();
            
            std::vector<uint32_t> signature;
            
            // 前10个文档使用相同的种子，这样不同客户端会生成相同的签名用于测试跨机构去重
            unsigned int seed;
            if (i < 10) {
                seed = config.local_params.seed + i; // 使用相同的种子
            } else {
                seed = config.local_params.seed + i + 1000; // 不同的种子
            }
            
            std::mt19937 gen(seed);
            std::uniform_int_distribution<uint32_t> dis(0, 4294967);
            
            for (int j = 0; j < num_hash; ++j) {
                signature.push_back(dis(gen));
            }
            
            doc.signature = signature;
            
            int bucket_id = 0;
            int h = num_hash / bucket_count;
            for (int b = 0; b < bucket_count; ++b) {
                unsigned int sum = 0;
                for (int k = b * h; k < (b + 1) * h; ++k) {
                    sum += signature[k];
                }
                bucket_id = sum % bucket_count;
            }
            doc.bucket_id = bucket_id;
            
            local_candidates.push_back(doc);
            id_mapping[doc.random_id] = doc.original_id;
        }
    }
    
    cout << "[Client] Local processing completed. Total candidates: " << local_candidates.size() << endl;
    
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
    
    std::ostringstream oss;
    oss << local_candidates.size() << "\n";
    
    for (const auto& doc : local_candidates) {
        std::vector<unsigned char> encrypted_sig = encrypt_signature(doc.signature);
        
        std::ostringstream sig_oss;
        sig_oss << std::hex << std::setfill('0');
        for (unsigned char c : encrypted_sig) {
            sig_oss << std::setw(2) << (int)c;
        }
        
        oss << doc.random_id << " " 
            << doc.bucket_id << " " 
            << sig_oss.str() << " "
            << config.client_id << "\n";
    }
    
    std::string data = oss.str();
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cout << "[Client] Socket creation error" << endl;
        return false;
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.server_port);
    
    if (inet_pton(AF_INET, config.server_address.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "[Client] Invalid address" << endl;
        close(sock);
        return false;
    }
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "[Client] Connection failed" << endl;
        close(sock);
        return false;
    }
    
    std::string message = "CANDIDATES " + config.client_id + " " + std::to_string(data.length()) + "\n";
    send(sock, message.c_str(), message.length(), 0);
    
    char buffer[4096] = {0};
    int valread = read(sock, buffer, sizeof(buffer) - 1);
    if (valread <= 0) {
        cout << "[Client] No READY response" << endl;
        close(sock);
        return false;
    }
    
    std::string response(buffer, valread);
    if (response.find("READY") == std::string::npos) {
        cout << "[Client] Server not ready: " << response << endl;
        close(sock);
        return false;
    }
    
    std::string data_header = "CANDIDATES_DATA " + std::to_string(data.length()) + "\n";
    send(sock, data_header.c_str(), data_header.length(), 0);
    
    memset(buffer, 0, sizeof(buffer));
    valread = read(sock, buffer, sizeof(buffer) - 1);
    if (valread <= 0) {
        cout << "[Client] No READY_FOR_DATA response" << endl;
        close(sock);
        return false;
    }
    
    std::string data_ready_response(buffer, valread);
    if (data_ready_response.find("READY_FOR_DATA") == std::string::npos) {
        cout << "[Client] Server not ready for data: " << data_ready_response << endl;
        close(sock);
        return false;
    }
    
    send(sock, data.c_str(), data.length(), 0);
    
    bool received_confirmation = false;
    for (int i = 0; i < 2; ++i) {
        memset(buffer, 0, sizeof(buffer));
        valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            break;
        }
        
        std::string data_response(buffer, valread);
        if (data_response.find("CANDIDATES_RECEIVED") != std::string::npos) {
            received_confirmation = true;
            break;
        }
    }
    
    if (!received_confirmation) {
        cout << "[Client] Data not received" << endl;
        close(sock);
        return false;
    }
    
    close(sock);
    cout << "[Client] Candidates sent successfully, total: " << local_candidates.size() << endl;
    return true;
}

bool FedClient::receive_and_process_results() {
    cout << "[Client] Receiving and processing results..." << endl;
    
    // 请求结果
    std::string message = "GET_RESULTS " + config.client_id + "\n";
    std::string response;
    
    if (!communicate_with_server(message, response)) {
        cout << "[Client] Failed to request results" << endl;
        return false;
    }
    
    // 解析结果
    std::istringstream iss(response);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "RESULTS") {
        int num_duplicates;
        iss >> num_duplicates;
        
        std::vector<std::string> global_duplicates;
        for (int i = 0; i < num_duplicates; ++i) {
            std::string random_id;
            iss >> random_id;
            
            // 将随机ID映射回原始ID
            auto it = id_mapping.find(random_id);
            if (it != id_mapping.end()) {
                global_duplicates.push_back(it->second);
            }
        }
        
        cout << "[Client] Received " << global_duplicates.size() << " cross-institution duplicates" << endl;
        return true;
    }
    
    return false;
}

bool FedClient::run() {
    if (!init()) return false;
    if (!process_local_data()) return false;
    if (!send_encrypted_candidates()) return false;
    
    // 触发全局相似度计算（最后一个客户端触发）
    if (config.client_id == "client2") {
        cout << "[Client] Triggering global similarity calculation..." << endl;
        if (!trigger_global_calculation()) {
            cout << "[Client] Failed to trigger global calculation" << endl;
            return false;
        }
    } else {
        // 其他客户端等待一会儿，让最后一个客户端完成计算
        cout << "[Client] Waiting for global similarity calculation to complete..." << endl;
        sleep(10); // 等待10秒，确保全局计算完成
    }
    
    if (!receive_and_process_results()) return false;
    return true;
}

std::string FedClient::generate_random_id() {
    return CryptoUtil::generate_random_id();
}

bool FedClient::trigger_global_calculation() {
    std::string message = "CALCULATE\n";
    std::string response;
    
    if (!communicate_with_server(message, response)) {
        cout << "[Client] Failed to trigger global calculation" << endl;
        return false;
    }
    
    if (response.find("CALCULATION_DONE") != std::string::npos) {
        cout << "[Client] Global similarity calculation completed" << endl;
        return true;
    } else {
        cout << "[Client] Global similarity calculation failed: " << response << endl;
        return false;
    }
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
    if (valread > 0) {
        response = string(buffer, valread);
    }
    
    close(sock);
    return true;
}