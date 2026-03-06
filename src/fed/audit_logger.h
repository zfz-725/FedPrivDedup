#ifndef AUDIT_LOGGER_H
#define AUDIT_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class AuditLevel {
    INFO,
    WARNING,
    ERROR,
    SECURITY
};

struct AuditEvent {
    std::string timestamp;
    AuditLevel level;
    std::string client_id;
    std::string action;
    std::string details;
    std::string ip_address;
};

class AuditLogger {
public:
    AuditLogger(const std::string& log_file = "audit.log");
    ~AuditLogger();
    
    bool init();
    
    void log(const AuditEvent& event);
    void log_info(const std::string& client_id, const std::string& action, const std::string& details = "");
    void log_warning(const std::string& client_id, const std::string& action, const std::string& details = "");
    void log_error(const std::string& client_id, const std::string& action, const std::string& details = "");
    void log_security(const std::string& client_id, const std::string& action, const std::string& details = "");
    
    void set_log_file(const std::string& log_file);
    
private:
    std::string log_file_;
    std::ofstream log_stream_;
    std::mutex mutex_;
    bool initialized_;
    
    std::string get_timestamp();
    std::string level_to_string(AuditLevel level);
    std::string event_to_string(const AuditEvent& event);
};

#endif // AUDIT_LOGGER_H
