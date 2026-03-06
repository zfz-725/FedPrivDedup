#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <vector>

enum class Permission {
    READ,
    WRITE,
    EXECUTE,
    ADMIN
};

struct ClientInfo {
    std::string client_id;
    std::string org_id;
    std::string api_key;
    std::set<Permission> permissions;
    std::string ip_address;
    std::string last_seen;
    bool is_active;
};

class AuthManager {
public:
    AuthManager();
    ~AuthManager();
    
    bool init();
    
    bool register_client(const std::string& client_id, const std::string& org_id, 
                         const std::string& api_key, const std::set<Permission>& permissions);
    
    bool authenticate_client(const std::string& client_id, const std::string& api_key);
    bool has_permission(const std::string& client_id, Permission permission);
    
    bool revoke_client(const std::string& client_id);
    bool update_permissions(const std::string& client_id, const std::set<Permission>& new_permissions);
    
    std::vector<ClientInfo> get_all_clients();
    ClientInfo get_client_info(const std::string& client_id);
    
    void update_last_seen(const std::string& client_id, const std::string& ip_address);
    
    bool generate_api_key(std::string& api_key);
    bool is_client_registered(const std::string& client_id);
    
private:
    std::map<std::string, ClientInfo> clients_;
    std::mutex mutex_;
    bool initialized_;
    
    std::string generate_random_string(size_t length);
};

#endif // AUTH_MANAGER_H
