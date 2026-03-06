#include "server.h"
#include "crypto.h"
#include "crypto_cuda.h"
#include "decrypt_cuda.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstdio>
#include <memory>

using namespace std;

// 将二进制数据转换为十六进制字符串
std::string bytes_to_hex(const std::string& data) {
    std::string hex_str;
    hex_str.reserve(data.length() * 2);
    
    for (size_t i = 0; i < data.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", c);
        hex_str += hex;
    }
    
    return hex_str;
}

FedServer::FedServer(const ServerConfig& config) : config(config) {
}

bool FedServer::init() {
    cout << "[Server] Initializing server..." << endl;
    
    // 初始化审计日志
    audit_logger_ = std::make_unique<AuditLogger>("audit.log");
    if (!audit_logger_->init()) {
        cerr << "[Server] Failed to initialize audit logger" << endl;
        return false;
    }
    
    // 初始化认证管理器
    auth_manager_ = std::make_unique<AuthManager>();
    if (!auth_manager_->init()) {
        cerr << "[Server] Failed to initialize auth manager" << endl;
        return false;
    }
    
    // 注册默认客户端（用于测试）
    std::set<Permission> default_permissions = {Permission::READ, Permission::WRITE};
    std::string api_key1, api_key2;
    auth_manager_->generate_api_key(api_key1);
    auth_manager_->generate_api_key(api_key2);
    
    auth_manager_->register_client("client1", "org1", api_key1, default_permissions);
    auth_manager_->register_client("client2", "org2", api_key2, default_permissions);
    
    audit_logger_->log_info("SYSTEM", "INIT", "Server initialized successfully");
    
    return true;
}

std::string FedServer::generate_client_key() {
    return CryptoUtil::generate_key();
}

std::string FedServer::get_global_params() {
    // 动态计算桶数 K = k√N
    int dynamic_bucket = calculate_dynamic_buckets(config.params.num_file);
    
    std::stringstream ss;
    ss << "PARAMS " 
       << config.params.num_hash << " " 
       << config.params.shingle_len << " " 
       << dynamic_bucket << " " 
       << config.params.seed << " " 
       << config.params.threshold << " " 
       << config.params.num_file;
    return ss.str();
}

void FedServer::handle_client_request(const std::string& request, std::string& response) {
    if (request.substr(0, 6) == "HELLO ") {
        std::istringstream iss(request.substr(6));
        std::string client_id, org_id;
        iss >> client_id >> org_id;
        std::string key = generate_client_key();
        client_keys[client_id] = key;
        
        std::string params = get_global_params();
        
        // 将密钥转换为十六进制字符串传输
        std::string hex_key = bytes_to_hex(key);
        
        response = params + " KEY " + hex_key + "\n";
        cout << "[Server] New client connected: " << client_id << " (" << org_id << ")" << endl;
        cout << "[Server] Sent response: '" << params << " KEY [" << hex_key.length() << " hex chars]'" << endl;
        
        audit_logger_->log_info(client_id, "CONNECT", "Client connected from " + org_id);
    } else if (request.substr(0, 15) == "CANDIDATES_DATA ") {
        std::istringstream iss(request.substr(15));
        size_t data_size;
        iss >> data_size;
        
        response = "READY_FOR_DATA\n";
        cout << "[Server] Ready to receive " << data_size << " bytes of candidates data" << endl;
        
        audit_logger_->log_info("UNKNOWN", "CANDIDATES_DATA", "Receiving " + std::to_string(data_size) + " bytes");
    } else if (request.substr(0, 1) != "C" && request.substr(0, 1) != "H" && request.substr(0, 1) != "G" && request.substr(0, 1) != "P" && request.substr(0, 1) != "R") {
        // 处理候选集数据
        cout << "[Server] Processing candidate data, request length: " << request.length() << endl;
        std::istringstream data_iss(request);
        int num_candidates;
        data_iss >> num_candidates;
        
        cout << "[Server] Expecting " << num_candidates << " candidates" << endl;
        
        int success_count = 0;
        for (int i = 0; i < num_candidates; ++i) {
            std::string doc_id, encrypted_sig_hex, client_id;
            int bucket_id;
            
            if (!(data_iss >> doc_id >> bucket_id >> encrypted_sig_hex >> client_id)) {
                cerr << "[Server] Failed to parse candidate " << i << endl;
                break;
            }
            
            std::vector<unsigned char> encrypted_sig;
            for (size_t j = 0; j < encrypted_sig_hex.length(); j += 2) {
                std::string byte_str = encrypted_sig_hex.substr(j, 2);
                unsigned char byte = static_cast<unsigned char>(stoul(byte_str, nullptr, 16));
                encrypted_sig.push_back(byte);
            }
            
            EncryptedCandidate candidate;
            candidate.client_id = client_id;
            candidate.doc_id = doc_id;
            candidate.encrypted_signature = encrypted_sig;
            candidate.bucket_id = bucket_id;
            candidate.org_id = client_id;
            
            candidates.push_back(candidate);
            bucket_candidates[bucket_id].push_back(candidate);
            success_count++;
        }
        
        response = "CANDIDATES_RECEIVED\n";
        cout << "[Server] Received " << success_count << " candidates" << endl;
        cout << "[Server] Total candidates in memory: " << candidates.size() << endl;
        
        audit_logger_->log_info("UNKNOWN", "CANDIDATES_RECEIVED", "Received " + std::to_string(success_count) + " candidates");
    } else if (request.substr(0, 10) == "CANDIDATES" && request.find("_DATA") == std::string::npos) {
        std::istringstream iss(request.substr(10));
        std::string client_id;
        size_t data_size;
        iss >> client_id >> data_size;
        
        response = "READY\n";
        cout << "[Server] Ready to receive candidates from " << client_id << endl;
        
        audit_logger_->log_info(client_id, "CANDIDATES", "Ready to receive " + std::to_string(data_size) + " bytes");
    } else if (request == "CALCULATE") {
        perform_global_similarity();
        response = "CALCULATION_DONE\n";
        
        audit_logger_->log_info("SYSTEM", "CALCULATE", "Global similarity calculation completed");
    } else if (request.find("GET_RESULTS ") == 0) {
        size_t space_pos = request.find(" ");
        if (space_pos != std::string::npos) {
            std::string client_id = request.substr(space_pos + 1);
            distribute_results();
            int duplicate_count = 0;
            std::string duplicate_ids;
            if (duplicate_docs.find(client_id) != duplicate_docs.end()) {
                duplicate_count = duplicate_docs[client_id].size();
                for (const auto& doc_id : duplicate_docs[client_id]) {
                    duplicate_ids += " " + doc_id;
                }
            }
            response = "RESULTS " + std::to_string(duplicate_count) + duplicate_ids + "\n";
            
            audit_logger_->log_info(client_id, "GET_RESULTS", "Sent " + std::to_string(duplicate_count) + " duplicates");
        } else {
            response = "UNKNOWN_COMMAND\n";
        }
    } else {
        response = "UNKNOWN_COMMAND\n";
        audit_logger_->log_warning("UNKNOWN", "UNKNOWN_COMMAND", "Unknown request: " + request);
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
            char buffer[8192] = {0};
            std::string request_buffer;
            bool waiting_for_data = false;
            size_t expected_data_size = 0;
            std::string data_buffer;
            
            cout << "[Server] New connection accepted" << endl;
            
            while (true) {
                int valread = read(new_socket, buffer, 8192);
                if (valread <= 0) {
                    // 连接关闭，处理剩余的请求
                    if (!request_buffer.empty() && !waiting_for_data) {
                        std::string response;
                        handle_client_request(request_buffer, response);
                        send(new_socket, response.c_str(), response.length(), 0);
                        cout << "[Server] Sent response: '" << response << "'" << endl;
                    }
                    break;
                }
                
                // 累积请求数据
                request_buffer.append(buffer, valread);
                
                if (waiting_for_data) {
                    // 正在等待数据块
                    data_buffer.append(buffer, valread);
                    
                    if (data_buffer.length() >= expected_data_size) {
                        // 收到完整的数据
                        std::string response;
                        handle_client_request(data_buffer.substr(0, expected_data_size), response);
                        send(new_socket, response.c_str(), response.length(), 0);
                        cout << "[Server] Sent response: '" << response << "'" << endl;
                        
                        waiting_for_data = false;
                        data_buffer.clear();
                        request_buffer.clear();
                    }
                } else {
                    // 处理完整的请求
                    size_t newline_pos = request_buffer.find('\n');
                    if (newline_pos != std::string::npos) {
                        std::string request = request_buffer.substr(0, newline_pos);
                        request_buffer = request_buffer.substr(newline_pos + 1);
                        
                        cout << "[Server] Received request: '" << request << "'" << endl;
                        
                        // 检查是否是数据传输请求
                        if (request.find("CANDIDATES_DATA ") == 0) {
                            std::istringstream iss(request.substr(15));
                            size_t data_size;
                            iss >> data_size;
                            
                            waiting_for_data = true;
                            expected_data_size = data_size;
                            data_buffer = request_buffer;
                            
                            std::string response = "READY_FOR_DATA\n";
                            send(new_socket, response.c_str(), response.length(), 0);
                            cout << "[Server] Sent response: '" << response << "'" << endl;
                            
                            request_buffer.clear();
                        } else {
                            std::string response;
                            handle_client_request(request, response);
                            send(new_socket, response.c_str(), response.length(), 0);
                            cout << "[Server] Sent response: '" << response << "'" << endl;
                        }
                    }
                }
                
                memset(buffer, 0, 8192);
            }
            close(new_socket);
        }).detach();
    }
    
    return true;
}

std::vector<uint32_t> FedServer::decrypt_signature(const std::vector<unsigned char>& encrypted, const std::string& client_id) {
    auto it = client_keys.find(client_id);
    if (it == client_keys.end()) {
        cerr << "[Server] No key found for client: " << client_id << endl;
        return std::vector<uint32_t>();
    }
    
    const std::string& key = it->second;
    std::vector<unsigned char> decrypted = CryptoUtil::decrypt(encrypted, key);
    
    std::vector<uint32_t> signature;
    int expected_size = config.params.num_hash;
    
    for (size_t i = 0; i + 4 <= decrypted.size() && signature.size() < expected_size; i += 4) {
        uint32_t val;
        memcpy(&val, &decrypted[i], 4);
        signature.push_back(val);
    }
    
    if (signature.size() != expected_size) {
        cerr << "[Server] Warning: Decrypted signature size " << signature.size() 
             << " does not match expected size " << expected_size << endl;
    }
    
    return signature;
}

double FedServer::calculate_similarity(const std::vector<uint32_t>& sig1, const std::vector<uint32_t>& sig2) {
    if (sig1.size() != sig2.size()) {
        return 0.0;
    }
    
    int match_count = 0;
    int num_hash = sig1.size();
    
    for (size_t i = 0; i < sig1.size(); i++) {
        if (sig1[i] == sig2[i]) {
            match_count++;
        }
    }
    
    return static_cast<double>(match_count) / num_hash;
}

int FedServer::calculate_dynamic_buckets(int num_files) {
    // 动态计算桶数 K = k√N，k=2
    int k = 2;
    int buckets = static_cast<int>(k * sqrt(num_files));
    // 确保至少有一个桶
    return std::max(1, buckets);
}

// 批量解密签名 - 使用GPU并行解密
std::vector<std::vector<uint32_t>> batch_decrypt_signatures(
    const std::vector<EncryptedCandidate>& candidates,
    const std::map<std::string, std::string>& client_keys
) {
    std::vector<std::vector<uint32_t>> decrypted_signatures;
    
    if (candidates.empty()) {
        return decrypted_signatures;
    }
    
    // 按客户端分组，以便使用对应的密钥
    std::map<std::string, std::vector<std::pair<size_t, std::vector<unsigned char>>>> client_groups;
    
    for (size_t i = 0; i < candidates.size(); ++i) {
        auto it = client_keys.find(candidates[i].client_id);
        if (it != client_keys.end()) {
            client_groups[candidates[i].client_id].push_back({i, candidates[i].encrypted_signature});
        }
    }
    
    decrypted_signatures.resize(candidates.size());
    
    // 对每个客户端的签名进行批量解密
    for (const auto& group : client_groups) {
        const std::string& client_id = group.first;
        auto key_it = client_keys.find(client_id);
        if (key_it == client_keys.end()) continue;
        
        std::vector<std::vector<unsigned char>> encrypted_sigs;
        std::vector<size_t> indices;
        
        for (const auto& item : group.second) {
            encrypted_sigs.push_back(item.second);
            indices.push_back(item.first);
        }
        
        // 使用GPU并行解密
        auto decrypted = gpu_decrypt_signatures(encrypted_sigs, key_it->second);
        
        // 将解密结果放回原位置
        for (size_t i = 0; i < indices.size() && i < decrypted.size(); ++i) {
            decrypted_signatures[indices[i]] = decrypted[i];
        }
    }
    
    return decrypted_signatures;
}

bool FedServer::perform_global_similarity() {
    cout << "[Server] Performing global similarity calculation..." << endl;
    cout << "[Server] Total candidates: " << candidates.size() << endl;
    cout << "[Server] Total buckets: " << bucket_candidates.size() << endl;
    
    bool use_gpu = cuda_available();
    if (use_gpu) {
        cout << "[Server] Using GPU acceleration for decryption and similarity calculation..." << endl;
    } else {
        cout << "[Server] CUDA not available, using CPU for calculation..." << endl;
    }
    
    cout << "[Server] Classifying candidates by bucket..." << endl;
    
    int total_duplicates = 0;
    
    for (const auto& bucket_entry : bucket_candidates) {
        int bucket_id = bucket_entry.first;
        const auto& bucket_candidates_list = bucket_entry.second;
        
        if (bucket_candidates_list.size() < 2) {
            continue;
        }
        
        cout << "[Server] Processing bucket " << bucket_id << " (" << bucket_candidates_list.size() << " documents)..." << endl;
        
        std::vector<std::vector<uint32_t>> signatures;
        std::vector<std::string> doc_ids;
        std::vector<std::string> client_ids;
        
        // 使用GPU批量解密（如果可用）
        if (use_gpu && bucket_candidates_list.size() >= 10) {
            cout << "[Server] Using GPU parallel decryption for bucket " << bucket_id << endl;
            auto decrypted_sigs = batch_decrypt_signatures(bucket_candidates_list, client_keys);
            
            for (size_t i = 0; i < bucket_candidates_list.size() && i < decrypted_sigs.size(); ++i) {
                if (decrypted_sigs[i].empty()) {
                    cerr << "[Server] Failed to decrypt signature for " << bucket_candidates_list[i].doc_id << endl;
                    continue;
                }
                
                signatures.push_back(decrypted_sigs[i]);
                doc_ids.push_back(bucket_candidates_list[i].doc_id);
                client_ids.push_back(bucket_candidates_list[i].client_id);
            }
        } else {
            // 使用CPU逐个解密
            for (const auto& candidate : bucket_candidates_list) {
                std::vector<uint32_t> signature = decrypt_signature(candidate.encrypted_signature, candidate.client_id);
                if (signature.empty()) {
                    cerr << "[Server] Failed to decrypt signature for " << candidate.doc_id << endl;
                    continue;
                }
                
                signatures.push_back(signature);
                doc_ids.push_back(candidate.doc_id);
                client_ids.push_back(candidate.client_id);
            }
        }
        
        if (signatures.size() < 2) {
            cout << "[Server] Bucket " << bucket_id << " skipped (insufficient valid signatures)" << endl;
            continue;
        }
        
        cout << "[Server] Decrypted " << signatures.size() << " signatures for comparison" << endl;
        
        try {
            // 使用GPU进行相似度计算
            std::vector<std::pair<int, int>> duplicates = gpu_calculate_similarity(signatures, config.params.threshold);
            
            int bucket_duplicates = 0;
            for (const auto& dup : duplicates) {
                int i = dup.first;
                int j = dup.second;
                
                // 只记录跨机构的重复
                if (client_ids[i] != client_ids[j]) {
                    // 避免重复添加
                    bool already_added = false;
                    for (const auto& existing : duplicate_docs[client_ids[i]]) {
                        if (existing == doc_ids[i]) {
                            already_added = true;
                            break;
                        }
                    }
                    
                    if (!already_added) {
                        duplicate_docs[client_ids[i]].push_back(doc_ids[i]);
                        duplicate_docs[client_ids[j]].push_back(doc_ids[j]);
                        bucket_duplicates++;
                        total_duplicates++;
                        
                        cout << "[Server] Found cross-institution duplicate: " 
                             << doc_ids[i] << " (" << client_ids[i] << ") and " 
                             << doc_ids[j] << " (" << client_ids[j] << ")" << endl;
                    }
                }
            }
            
            cout << "[Server] Bucket " << bucket_id << ": found " << bucket_duplicates << " cross-institution duplicates" << endl;
            
        } catch (const std::exception& e) {
            cerr << "[Server] Error processing bucket " << bucket_id << ": " << e.what() << endl;
        }
        
        // 即时销毁机制：立即清除敏感数据
        signatures.clear();
        signatures.shrink_to_fit();
        doc_ids.clear();
        doc_ids.shrink_to_fit();
        client_ids.clear();
        client_ids.shrink_to_fit();
        
        cout << "[Server] Bucket " << bucket_id << " processed, sensitive data destroyed immediately" << endl;
    }
    
    cout << "[Server] Global similarity calculation completed. Total cross-institution duplicates: " << total_duplicates << endl;
    return true;
}

bool FedServer::distribute_results() {
    cout << "[Server] Distributing results..." << endl;
    return true;
}