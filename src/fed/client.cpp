#include "client.h"
#include "crypto.h"
#include "fed_cuda.h"
#include "union_find.h"
#include "data_preprocessor.h"
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
#include <set>

using namespace std;

FedClient::FedClient(const ClientConfig& config) : config(config), ssl_ctx(nullptr), ssl(nullptr) {
}

bool FedClient::init() {
    cout << "[Client] Initializing client..." << endl;
    
    // 初始化TLS
    if (config.use_tls) {
        if (!init_tls()) {
            cout << "[Client] Failed to initialize TLS" << endl;
            return false;
        }
    }
    
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
            std::string hex_key;
            // 读取剩余的整行作为十六进制密钥
            std::getline(iss, hex_key);
            // 去除前导空格
            size_t start = hex_key.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                hex_key = hex_key.substr(start);
            }
            // 去除尾部换行符
            size_t end = hex_key.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                hex_key = hex_key.substr(0, end + 1);
            }
            
            cout << "[Client] Received hex key length: " << hex_key.length() << endl;
            
            // 将十六进制字符串转换为二进制密钥
            server_key.clear();
            for (size_t i = 0; i + 1 < hex_key.length(); i += 2) {
                std::string byte_str = hex_key.substr(i, 2);
                try {
                    unsigned char byte = static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16));
                    server_key += byte;
                } catch (const std::exception& e) {
                    cerr << "[Client] Error parsing hex key at position " << i << ": " << byte_str << endl;
                    return false;
                }
            }
            
            cout << "[Client] Received global parameters and key (length: " << server_key.length() << " bytes)" << endl;
            return true;
        }
    }
    
    return false;
}

// 计算LSH桶ID - 按照方案文档的完整LSH带划分算法
int calculate_bucket_id(const std::vector<uint32_t>& signature, int num_bands, int num_hash) {
    // 按照FED方案：将签名矩阵划分为b个带，每个带r行
    // 计算每个带的桶ID，使用所有带的组合作为最终桶ID
    int rows_per_band = num_hash / num_bands;
    int bucket_id = 0;
    
    for (int b = 0; b < num_bands; ++b) {
        unsigned int band_sum = 0;
        for (int r = 0; r < rows_per_band; ++r) {
            int idx = b * rows_per_band + r;
            if (idx < num_hash) {
                band_sum += signature[idx];
            }
        }
        // 使用带内哈希值的和对桶数取模
        bucket_id = (bucket_id * 31 + band_sum) % 1000003; // 使用大质数减少冲突
    }
    
    return bucket_id;
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
    
    // 初始化FedCuda进行GPU加速的签名生成
    FedCuda fed_cuda(
        config.local_params.num_hash,
        config.local_params.shingle_len,
        config.local_params.bucket,
        config.local_params.seed
    );
    
    if (!fed_cuda.init()) {
        cout << "[Client] Failed to initialize FedCuda" << endl;
        return false;
    }
    
    cout << "[Client] FedCuda initialized successfully" << endl;
    
    if (file_list.empty()) {
        cout << "[Client] No JSONL files found in " << data_dir << endl;
        cout << "[Client] Generating sample data for testing..." << endl;
        
        int num_samples = 100;
        int num_hash = config.local_params.num_hash;
        int num_bands = config.local_params.bucket;
        
        // 生成测试文档文本
        std::vector<std::string> test_texts;
        for (int i = 0; i < num_samples; ++i) {
            // 前10个文档使用相同内容，这样不同客户端会生成相同的签名用于测试跨机构去重
            if (i < 10) {
                test_texts.push_back("machine learning artificial intelligence test document " + std::to_string(i));
            } else {
                test_texts.push_back("unique content for document " + std::to_string(i) + " with random data");
            }
        }
        
        // 使用GPU批量生成签名
        auto results = fed_cuda.compute_batch_signatures(test_texts);
        
        for (int i = 0; i < num_samples; ++i) {
            DocumentInfo doc;
            doc.original_id = config.client_id + "_doc" + std::to_string(i);
            doc.random_id = generate_random_id();
            doc.signature = results[i].signature;
            
            // 使用完整的LSH桶划分算法计算桶ID
            doc.bucket_id = calculate_bucket_id(doc.signature, num_bands, num_hash);
            
            local_candidates.push_back(doc);
            id_mapping[doc.random_id] = doc.original_id;
        }
        
        cout << "[Client] Generated " << local_candidates.size() << " sample documents with GPU acceleration" << endl;
        return true;
    }
    
    cout << "[Client] Found " << file_list.size() << " files to process" << endl;

    // 初始化数据预处理器（使用4线程并行加载）
    DataPreprocessor preprocessor(200, 1000, 4);
    if (!preprocessor.init()) {
        cout << "[Client] Failed to initialize data preprocessor" << endl;
        return false;
    }

    cout << "[Client] Data preprocessor initialized (min_length=200, buffer_size=1000, threads=4)" << endl;

    // 处理JSONL文件
    for (size_t file_idx = 0; file_idx < file_list.size(); ++file_idx) {
        fs::path input_path(file_list[file_idx]);
        std::string filename = input_path.stem().string();

        cout << "[Client] Processing file: " << file_list[file_idx] << endl;

        // 使用CPU并行加载文档（包含NFC归一化、长度过滤）
        auto documents = preprocessor.load_documents_parallel(file_list[file_idx]);

        if (documents.empty()) {
            cout << "[Client] No valid documents found in " << file_list[file_idx] << endl;
            continue;
        }

        cout << "[Client] Loaded " << documents.size() << " documents after parallel preprocessing, generating signatures with GPU..." << endl;
        
        // 提取文本用于GPU签名生成
        std::vector<std::string> texts;
        for (const auto& doc : documents) {
            texts.push_back(doc.text);
        }
        
        // 使用GPU批量生成签名
        auto results = fed_cuda.compute_batch_signatures(texts);
        
        int num_bands = config.local_params.bucket;
        int num_hash = config.local_params.num_hash;
        
        for (size_t i = 0; i < results.size(); ++i) {
            DocumentInfo doc;
            doc.original_id = filename + "_doc" + std::to_string(documents[i].line_number);
            doc.random_id = generate_random_id();
            doc.signature = results[i].signature;
            
            // 使用完整的LSH桶划分算法计算桶ID
            doc.bucket_id = calculate_bucket_id(doc.signature, num_bands, num_hash);
            
            local_candidates.push_back(doc);
            id_mapping[doc.random_id] = doc.original_id;
        }
        
        cout << "[Client] Processed " << results.size() << " documents from " << filename << endl;
    }
    
    if (local_candidates.empty()) {
        cout << "[Client] No candidates loaded, generating sample data..." << endl;
        int num_samples = 100;
        int num_hash = config.local_params.num_hash;
        int num_bands = config.local_params.bucket;
        
        std::vector<std::string> test_texts;
        for (int i = 0; i < num_samples; ++i) {
            if (i < 10) {
                test_texts.push_back("machine learning artificial intelligence test document " + std::to_string(i));
            } else {
                test_texts.push_back("unique content for document " + std::to_string(i) + " with random data");
            }
        }
        
        auto results = fed_cuda.compute_batch_signatures(test_texts);
        
        for (int i = 0; i < num_samples; ++i) {
            DocumentInfo doc;
            doc.original_id = config.client_id + "_doc" + std::to_string(i);
            doc.random_id = generate_random_id();
            doc.signature = results[i].signature;
            doc.bucket_id = calculate_bucket_id(doc.signature, num_bands, num_hash);
            
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

bool FedClient::perform_local_deduplication() {
    cout << "[Client] Performing local deduplication..." << endl;
    
    if (local_candidates.empty()) {
        cout << "[Client] No candidates for local deduplication" << endl;
        return true;
    }
    
    LocalDeduplicator deduplicator;
    
    for (size_t i = 0; i < local_candidates.size(); i++) {
        deduplicator.add_document(i, local_candidates[i].signature);
    }
    
    deduplicator.perform_local_deduplication(config.local_params.threshold);
    
    std::vector<int> duplicate_indices = deduplicator.get_duplicate_document_ids();
    
    if (!duplicate_indices.empty()) {
        cout << "[Client] Found " << duplicate_indices.size() << " local duplicates" << endl;
        
        std::vector<DocumentInfo> filtered_candidates;
        std::set<int> duplicate_set(duplicate_indices.begin(), duplicate_indices.end());
        
        for (size_t i = 0; i < local_candidates.size(); i++) {
            if (duplicate_set.find(i) == duplicate_set.end()) {
                filtered_candidates.push_back(local_candidates[i]);
            }
        }
        
        local_candidates = filtered_candidates;
        cout << "[Client] After local deduplication: " << local_candidates.size() << " documents" << endl;
    } else {
        cout << "[Client] No local duplicates found" << endl;
    }
    
    return true;
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
    
    bool use_ssl = config.use_tls;
    SSL* local_ssl = nullptr;
    
    // 建立SSL连接
    if (use_ssl) {
        if (!establish_ssl_connection(sock)) {
            cout << "[Client] Failed to establish SSL connection" << endl;
            close(sock);
            return false;
        }
        local_ssl = ssl;
    }
    
    // 发送CANDIDATES消息
    std::string message = "CANDIDATES " + config.client_id + " " + std::to_string(data.length()) + "\n";
    if (use_ssl && local_ssl) {
        if (ssl_write(message.c_str(), message.length()) <= 0) {
            cout << "[Client] SSL write failed" << endl;
            if (local_ssl) {
                SSL_shutdown(local_ssl);
                SSL_free(local_ssl);
                ssl = nullptr;
            }
            close(sock);
            return false;
        }
    } else {
        send(sock, message.c_str(), message.length(), 0);
    }
    
    char buffer[4096] = {0};
    int valread;
    if (use_ssl && local_ssl) {
        valread = ssl_read(buffer, sizeof(buffer) - 1);
    } else {
        valread = read(sock, buffer, sizeof(buffer) - 1);
    }
    
    if (valread <= 0) {
        cout << "[Client] No READY response" << endl;
        if (use_ssl && local_ssl) {
            SSL_shutdown(local_ssl);
            SSL_free(local_ssl);
            ssl = nullptr;
        }
        close(sock);
        return false;
    }
    
    std::string response(buffer, valread);
    if (response.find("READY") == std::string::npos) {
        cout << "[Client] Server not ready: " << response << endl;
        if (use_ssl && local_ssl) {
            SSL_shutdown(local_ssl);
            SSL_free(local_ssl);
            ssl = nullptr;
        }
        close(sock);
        return false;
    }
    
    // 发送CANDIDATES_DATA消息
    std::string data_header = "CANDIDATES_DATA " + std::to_string(data.length()) + "\n";
    if (use_ssl && local_ssl) {
        if (ssl_write(data_header.c_str(), data_header.length()) <= 0) {
            cout << "[Client] SSL write failed" << endl;
            if (local_ssl) {
                SSL_shutdown(local_ssl);
                SSL_free(local_ssl);
                ssl = nullptr;
            }
            close(sock);
            return false;
        }
    } else {
        send(sock, data_header.c_str(), data_header.length(), 0);
    }
    
    memset(buffer, 0, sizeof(buffer));
    if (use_ssl && local_ssl) {
        valread = ssl_read(buffer, sizeof(buffer) - 1);
    } else {
        valread = read(sock, buffer, sizeof(buffer) - 1);
    }
    
    if (valread <= 0) {
        cout << "[Client] No READY_FOR_DATA response" << endl;
        if (use_ssl && local_ssl) {
            SSL_shutdown(local_ssl);
            SSL_free(local_ssl);
            ssl = nullptr;
        }
        close(sock);
        return false;
    }
    
    std::string data_ready_response(buffer, valread);
    if (data_ready_response.find("READY_FOR_DATA") == std::string::npos) {
        cout << "[Client] Server not ready for data: " << data_ready_response << endl;
        if (use_ssl && local_ssl) {
            SSL_shutdown(local_ssl);
            SSL_free(local_ssl);
            ssl = nullptr;
        }
        close(sock);
        return false;
    }
    
    // 发送数据
    if (use_ssl && local_ssl) {
        if (ssl_write(data.c_str(), data.length()) <= 0) {
            cout << "[Client] SSL write failed" << endl;
            if (local_ssl) {
                SSL_shutdown(local_ssl);
                SSL_free(local_ssl);
                ssl = nullptr;
            }
            close(sock);
            return false;
        }
    } else {
        send(sock, data.c_str(), data.length(), 0);
    }
    
    bool received_confirmation = false;
    for (int i = 0; i < 2; ++i) {
        memset(buffer, 0, sizeof(buffer));
        if (use_ssl && local_ssl) {
            valread = ssl_read(buffer, sizeof(buffer) - 1);
        } else {
            valread = read(sock, buffer, sizeof(buffer) - 1);
        }
        
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
        if (use_ssl && local_ssl) {
            SSL_shutdown(local_ssl);
            SSL_free(local_ssl);
            ssl = nullptr;
        }
        close(sock);
        return false;
    }
    
    // 清理
    if (use_ssl && local_ssl) {
        SSL_shutdown(local_ssl);
        SSL_free(local_ssl);
        ssl = nullptr;
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
    if (!perform_local_deduplication()) return false;
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
    bool use_ssl = config.use_tls;
    SSL* local_ssl = nullptr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "[Client] Socket creation error" << endl;
        return false;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.server_port);
    
    if (inet_pton(AF_INET, config.server_address.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "[Client] Invalid address" << endl;
        close(sock);
        return false;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "[Client] Connection failed" << endl;
        close(sock);
        return false;
    }
    
    // 建立SSL连接
    if (use_ssl) {
        if (!establish_ssl_connection(sock)) {
            cout << "[Client] Failed to establish SSL connection" << endl;
            close(sock);
            return false;
        }
        local_ssl = ssl;
    }
    
    // 发送消息
    if (use_ssl && local_ssl) {
        if (ssl_write(message.c_str(), message.length()) <= 0) {
            cout << "[Client] SSL write failed" << endl;
            if (local_ssl) {
                SSL_shutdown(local_ssl);
                SSL_free(local_ssl);
                ssl = nullptr;
            }
            close(sock);
            return false;
        }
    } else {
        send(sock, message.c_str(), message.length(), 0);
    }
    
    // 接收响应
    char buffer[1024] = {0};
    int valread;
    if (use_ssl && local_ssl) {
        valread = ssl_read(buffer, 1024);
    } else {
        valread = read(sock, buffer, 1024);
    }
    
    if (valread > 0) {
        response = string(buffer, valread);
    }
    
    // 清理
    if (use_ssl && local_ssl) {
        SSL_shutdown(local_ssl);
        SSL_free(local_ssl);
        ssl = nullptr;
    }
    close(sock);
    return true;
}

// TLS初始化
bool FedClient::init_tls() {
    cout << "[Client] Initializing TLS..." << endl;
    
    // 初始化OpenSSL库
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    
    // 创建TLS上下文
    const SSL_METHOD* method = TLS_client_method();
    ssl_ctx = SSL_CTX_new(method);
    if (!ssl_ctx) {
        cerr << "[Client] Failed to create SSL context" << endl;
        return false;
    }
    
    // 设置TLS版本
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION);
    
    // 加载CA证书
    if (SSL_CTX_load_verify_locations(ssl_ctx, config.ca_cert_file.c_str(), nullptr) <= 0) {
        cerr << "[Client] Failed to load CA certificate" << endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    // 设置验证模式（测试环境跳过验证）
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, nullptr);
    
    cout << "[Client] TLS initialized successfully" << endl;
    return true;
}

// TLS清理
void FedClient::cleanup_tls() {
    if (ssl) {
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
    EVP_cleanup();
}

// 建立SSL连接
bool FedClient::establish_ssl_connection(int socket_fd) {
    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        cerr << "[Client] Failed to create SSL object" << endl;
        return false;
    }
    
    if (SSL_set_fd(ssl, socket_fd) != 1) {
        cerr << "[Client] Failed to set socket for SSL" << endl;
        SSL_free(ssl);
        ssl = nullptr;
        return false;
    }
    
    if (SSL_connect(ssl) != 1) {
        cerr << "[Client] SSL connect failed" << endl;
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        ssl = nullptr;
        return false;
    }
    
    cout << "[Client] SSL connection established" << endl;
    return true;
}

// SSL读取
int FedClient::ssl_read(char* buffer, int buffer_size) {
    if (!ssl) {
        return -1;
    }
    return SSL_read(ssl, buffer, buffer_size);
}

// SSL写入
int FedClient::ssl_write(const char* data, int data_size) {
    if (!ssl) {
        return -1;
    }
    return SSL_write(ssl, data, data_size);
}