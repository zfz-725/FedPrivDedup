#include "audit_logger.h"
#include <iostream>
#include <ctime>

AuditLogger::AuditLogger(const std::string& log_file)
    : log_file_(log_file), initialized_(false) {
}

AuditLogger::~AuditLogger() {
    if (log_stream_.is_open()) {
        log_stream_.close();
    }
}

bool AuditLogger::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    log_stream_.open(log_file_, std::ios::app);
    if (!log_stream_.is_open()) {
        std::cerr << "[AuditLogger] Failed to open log file: " << log_file_ << std::endl;
        return false;
    }
    
    initialized_ = true;
    
    // 直接记录初始化日志，避免递归调用
    AuditEvent event;
    event.timestamp = get_timestamp();
    event.level = AuditLevel::INFO;
    event.client_id = "SYSTEM";
    event.action = "INIT";
    event.details = "Audit logger initialized";
    
    std::string log_entry = event_to_string(event);
    log_stream_ << log_entry << std::endl;
    log_stream_.flush();
    std::cout << log_entry << std::endl;
    
    return true;
}

std::string AuditLogger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

std::string AuditLogger::level_to_string(AuditLevel level) {
    switch (level) {
        case AuditLevel::INFO:
            return "INFO";
        case AuditLevel::WARNING:
            return "WARNING";
        case AuditLevel::ERROR:
            return "ERROR";
        case AuditLevel::SECURITY:
            return "SECURITY";
        default:
            return "UNKNOWN";
    }
}

std::string AuditLogger::event_to_string(const AuditEvent& event) {
    std::stringstream ss;
    ss << "[" << event.timestamp << "] "
       << "[" << level_to_string(event.level) << "] "
       << "[Client: " << event.client_id << "] "
       << "[Action: " << event.action << "] ";
    
    if (!event.details.empty()) {
        ss << event.details;
    }
    
    if (!event.ip_address.empty()) {
        ss << " [IP: " << event.ip_address << "]";
    }
    
    return ss.str();
}

void AuditLogger::log(const AuditEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !log_stream_.is_open()) {
        std::cerr << "[AuditLogger] Logger not initialized" << std::endl;
        return;
    }
    
    std::string log_entry = event_to_string(event);
    log_stream_ << log_entry << std::endl;
    log_stream_.flush();
    
    std::cout << log_entry << std::endl;
}

void AuditLogger::log_info(const std::string& client_id, const std::string& action, const std::string& details) {
    AuditEvent event;
    event.timestamp = get_timestamp();
    event.level = AuditLevel::INFO;
    event.client_id = client_id;
    event.action = action;
    event.details = details;
    
    log(event);
}

void AuditLogger::log_warning(const std::string& client_id, const std::string& action, const std::string& details) {
    AuditEvent event;
    event.timestamp = get_timestamp();
    event.level = AuditLevel::WARNING;
    event.client_id = client_id;
    event.action = action;
    event.details = details;
    
    log(event);
}

void AuditLogger::log_error(const std::string& client_id, const std::string& action, const std::string& details) {
    AuditEvent event;
    event.timestamp = get_timestamp();
    event.level = AuditLevel::ERROR;
    event.client_id = client_id;
    event.action = action;
    event.details = details;
    
    log(event);
}

void AuditLogger::log_security(const std::string& client_id, const std::string& action, const std::string& details) {
    AuditEvent event;
    event.timestamp = get_timestamp();
    event.level = AuditLevel::SECURITY;
    event.client_id = client_id;
    event.action = action;
    event.details = details;
    
    log(event);
}

void AuditLogger::set_log_file(const std::string& log_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_stream_.is_open()) {
        log_stream_.close();
    }
    
    log_file_ = log_file;
    log_stream_.open(log_file_, std::ios::app);
    
    if (!log_stream_.is_open()) {
        std::cerr << "[AuditLogger] Failed to open new log file: " << log_file_ << std::endl;
        initialized_ = false;
    } else {
        initialized_ = true;
        log_info("SYSTEM", "CHANGE_LOG_FILE", "Log file changed to: " + log_file);
    }
}
