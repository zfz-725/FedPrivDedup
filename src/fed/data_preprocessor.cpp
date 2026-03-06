#include "data_preprocessor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <chrono>

#ifdef USE_ICU
#include <unicode/unistr.h>
#include <unicode/ucnv.h>
#endif

DataPreprocessor::DataPreprocessor(int min_length, int buffer_size, int num_threads)
    : min_length_(min_length), buffer_size_(buffer_size), num_threads_(num_threads),
      initialized_(false), loading_done_(false), processing_done_(false), active_processors_(0) {
}

DataPreprocessor::~DataPreprocessor() {
}

bool DataPreprocessor::init() {
    text_buffer_.reserve(buffer_size_);
    index_buffer_.reserve(buffer_size_);
    initialized_ = true;
    return true;
}

bool DataPreprocessor::normalize_nfc(std::string& text) {
#ifdef USE_ICU
    try {
        icu::UnicodeString unicode_text = icu::UnicodeString::fromUTF8(text);
        icu::UnicodeString normalized;
        UErrorCode status = U_ZERO_ERROR;
        
        normalized = unicode_text.normalize(UNORM_NFC, status);
        
        if (U_FAILURE(status)) {
            std::cerr << "[Preprocessor] NFC normalization failed: " << u_errorName(status) << std::endl;
            return false;
        }
        
        std::string result;
        normalized.toUTF8String(result);
        text = result;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Preprocessor] NFC normalization error: " << e.what() << std::endl;
        return false;
    }
#else
    std::cout << "[Preprocessor] ICU not available, skipping NFC normalization" << std::endl;
    return true;
#endif
}

bool DataPreprocessor::filter_by_length(const std::string& text) {
    return text.length() >= static_cast<size_t>(min_length_);
}

std::vector<Document> DataPreprocessor::load_documents_from_jsonl(const std::string& filepath) {
    std::vector<Document> documents;
    
    std::ifstream infile(filepath);
    if (!infile) {
        std::cerr << "[Preprocessor] Failed to open file: " << filepath << std::endl;
        return documents;
    }
    
    std::string line;
    int line_number = 0;
    
    while (std::getline(infile, line)) {
        line_number++;
        
        std::string text;
        
        size_t text_pos = line.find("\"text\":");
        if (text_pos != std::string::npos) {
            size_t start = line.find("\"", text_pos + 7);
            size_t end = line.find("\"", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                text = line.substr(start + 1, end - start - 1);
            }
        }
        
        if (text.empty()) {
            continue;
        }
        
        Document doc;
        doc.text = text;
        doc.original_id = "doc_" + std::to_string(line_number);
        doc.line_number = line_number;
        
        text_buffer_.push_back(text);
        index_buffer_.push_back(line_number);
        
        if (text_buffer_.size() >= static_cast<size_t>(buffer_size_)) {
            flush_buffer(documents);
        }
    }
    
    if (!text_buffer_.empty()) {
        flush_buffer(documents);
    }
    
    infile.close();
    
    std::cout << "[Preprocessor] Loaded " << documents.size() << " documents from " << filepath << std::endl;
    
    return documents;
}

std::vector<Document> DataPreprocessor::load_documents_from_buffer(const std::vector<std::string>& buffer) {
    std::vector<Document> documents;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        std::string text = buffer[i];
        
        if (text.empty()) {
            continue;
        }
        
        Document doc;
        doc.text = text;
        doc.original_id = "doc_" + std::to_string(i);
        doc.line_number = i;
        
        text_buffer_.push_back(text);
        index_buffer_.push_back(i);
        
        if (text_buffer_.size() >= static_cast<size_t>(buffer_size_)) {
            flush_buffer(documents);
        }
    }
    
    if (!text_buffer_.empty()) {
        flush_buffer(documents);
    }
    
    return documents;
}

bool DataPreprocessor::flush_buffer(std::vector<Document>& documents) {
    if (text_buffer_.empty()) {
        return true;
    }
    
    std::cout << "[Preprocessor] Processing buffer with " << text_buffer_.size() << " documents..." << std::endl;
    
    for (size_t i = 0; i < text_buffer_.size(); ++i) {
        std::string text = text_buffer_[i];
        
        if (!normalize_nfc(text)) {
            std::cerr << "[Preprocessor] Failed to normalize document " << index_buffer_[i] << std::endl;
            continue;
        }
        
        if (!filter_by_length(text)) {
            std::cout << "[Preprocessor] Filtered short document " << index_buffer_[i] 
                      << " (length: " << text.length() << ")" << std::endl;
            continue;
        }
        
        Document doc;
        doc.text = text;
        doc.original_id = "doc_" + std::to_string(index_buffer_[i]);
        doc.line_number = index_buffer_[i];
        
        documents.push_back(doc);
    }
    
    text_buffer_.clear();
    index_buffer_.clear();
    
    return true;
}

void DataPreprocessor::set_min_length(int min_length) {
    min_length_ = min_length;
}

void DataPreprocessor::set_buffer_size(int buffer_size) {
    buffer_size_ = buffer_size;
}

void DataPreprocessor::set_num_threads(int num_threads) {
    num_threads_ = num_threads;
}

// CPU并行加载实现
Document DataPreprocessor::parse_and_process(const RawDocument& raw) {
    Document doc;
    doc.line_number = raw.line_number;
    doc.original_id = "doc_" + std::to_string(raw.line_number);
    
    // 解析JSON提取text字段
    std::string text;
    size_t text_pos = raw.raw_line.find("\"text\":");
    if (text_pos != std::string::npos) {
        size_t start = raw.raw_line.find("\"", text_pos + 7);
        size_t end = raw.raw_line.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            text = raw.raw_line.substr(start + 1, end - start - 1);
        }
    }
    
    // NFC归一化
    normalize_nfc(text);
    
    doc.text = text;
    return doc;
}

void DataPreprocessor::load_worker(const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile) {
        std::cerr << "[Preprocessor] Failed to open file: " << filepath << std::endl;
        loading_done_ = true;
        return;
    }
    
    std::string line;
    int line_number = 0;
    
    while (std::getline(infile, line)) {
        line_number++;
        
        RawDocument raw;
        raw.raw_line = line;
        raw.line_number = line_number;
        
        {
            std::lock_guard<std::mutex> lock(raw_mutex_);
            raw_queue_.push(raw);
        }
    }
    
    infile.close();
    loading_done_ = true;
    
    std::cout << "[Preprocessor] Load worker finished, loaded " << line_number << " lines" << std::endl;
}

void DataPreprocessor::process_worker(std::vector<Document>& results) {
    active_processors_++;
    
    std::vector<Document> local_results;
    
    while (true) {
        RawDocument raw;
        bool has_data = false;
        
        {
            std::lock_guard<std::mutex> lock(raw_mutex_);
            if (!raw_queue_.empty()) {
                raw = raw_queue_.front();
                raw_queue_.pop();
                has_data = true;
            } else if (loading_done_) {
                // 队列为空且加载完成，退出
                break;
            }
        }
        
        if (has_data) {
            Document doc = parse_and_process(raw);
            
            // 长度过滤
            if (!doc.text.empty() && filter_by_length(doc.text)) {
                local_results.push_back(doc);
            } else if (!doc.text.empty()) {
                std::cout << "[Preprocessor] Filtered short document " << doc.line_number 
                          << " (length: " << doc.text.length() << ")" << std::endl;
            }
        } else {
            // 暂时没有数据，短暂休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // 将本地结果合并到全局结果
    {
        std::lock_guard<std::mutex> lock(processed_mutex_);
        results.insert(results.end(), local_results.begin(), local_results.end());
    }
    
    active_processors_--;
}

std::vector<Document> DataPreprocessor::load_documents_parallel(const std::string& filepath) {
    std::vector<Document> documents;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "[Preprocessor] Starting parallel loading with " << num_threads_ << " threads..." << std::endl;
    
    // 重置状态
    loading_done_ = false;
    processing_done_ = false;
    active_processors_ = 0;
    
    // 清空队列
    while (!raw_queue_.empty()) {
        raw_queue_.pop();
    }
    
    // 启动加载线程
    std::thread loader(&DataPreprocessor::load_worker, this, filepath);
    
    // 启动处理线程
    std::vector<std::thread> processors;
    for (int i = 0; i < num_threads_; ++i) {
        processors.emplace_back(&DataPreprocessor::process_worker, this, std::ref(documents));
    }
    
    // 等待加载线程完成
    loader.join();
    
    // 等待所有处理线程完成
    for (auto& t : processors) {
        t.join();
    }
    
    processing_done_ = true;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "[Preprocessor] Parallel loading completed in " << duration.count() << " ms" << std::endl;
    std::cout << "[Preprocessor] Loaded " << documents.size() << " documents from " << filepath << std::endl;
    
    return documents;
}
