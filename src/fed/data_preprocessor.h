#ifndef DATA_PREPROCESSOR_H
#define DATA_PREPROCESSOR_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

struct Document {
    std::string text;
    std::string original_id;
    int line_number;
};

struct RawDocument {
    std::string raw_line;
    int line_number;
};

class DataPreprocessor {
public:
    DataPreprocessor(int min_length = 200, int buffer_size = 10000, int num_threads = 4);
    ~DataPreprocessor();
    
    bool init();
    
    bool normalize_nfc(std::string& text);
    bool filter_by_length(const std::string& text);
    
    std::vector<Document> load_documents_from_jsonl(const std::string& filepath);
    std::vector<Document> load_documents_from_buffer(const std::vector<std::string>& buffer);
    
    // CPU并行加载
    std::vector<Document> load_documents_parallel(const std::string& filepath);
    
    void set_min_length(int min_length);
    void set_buffer_size(int buffer_size);
    void set_num_threads(int num_threads);
    
private:
    int min_length_;
    int buffer_size_;
    int num_threads_;
    bool initialized_;
    
    std::vector<std::string> text_buffer_;
    std::vector<int> index_buffer_;
    
    // 并行处理相关
    std::queue<RawDocument> raw_queue_;
    std::queue<Document> processed_queue_;
    std::mutex raw_mutex_;
    std::mutex processed_mutex_;
    std::atomic<bool> loading_done_;
    std::atomic<bool> processing_done_;
    std::atomic<int> active_processors_;
    
    bool flush_buffer(std::vector<Document>& documents);
    
    // 并行处理函数
    void load_worker(const std::string& filepath);
    void process_worker(std::vector<Document>& results);
    Document parse_and_process(const RawDocument& raw);
};

#endif // DATA_PREPROCESSOR_H
