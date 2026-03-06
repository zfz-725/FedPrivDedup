#include "auth_manager.h"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

AuthManager::AuthManager()
    : initialized_(false) {
}

AuthManager::~AuthManager() {
}

bool AuthManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = true;
    return true;
}

std::string AuthManager::generate_random_string(size_t length) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    
    return result;
}

bool AuthManager::generate_api_key(std::string& api_key) {
    api_key = generate_random_string(32);
    return true;
}

bool AuthManager::register_client(const std::string& client_id, const std::string& org_id,
                                  const std::string& api_key, const std::set<Permission>& permissions) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        std::cerr << "[AuthManager] Not initialized" << std::endl;
        return false;
    }
    
    if (clients_.find(client_id) != clients_.end()) {
        std::cerr << "[AuthManager] Client already exists: " << client_id << std::endl;
        return false;
    }
    
    ClientInfo info;
    info.client_id = client_id;
    info.org_id = org_id;
    info.api_key = api_key;
    info.permissions = permissions;
    info.is_active = true;
    info.last_seen = "";
    
    clients_[client_id] = info;
    
    std::cout << "[AuthManager] Registered client: " << client_id << " (org: " << org_id << ")" << std::endl;
    
    return true;
}

bool AuthManager::authenticate_client(const std::string& client_id, const std::string& api_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        std::cerr << "[AuthManager] Client not found: " << client_id << std::endl;
        return false;
    }
    
    if (!it->second.is_active) {
        std::cerr << "[AuthManager] Client is inactive: " << client_id << std::endl;
        return false;
    }
    
    if (it->second.api_key != api_key) {
        std::cerr << "[AuthManager] Invalid API key for client: " << client_id << std::endl;
        return false;
    }
    
    return true;
}

bool AuthManager::has_permission(const std::string& client_id, Permission permission) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return false;
    }
    
    return it->second.permissions.find(permission) != it->second.permissions.end();
}

bool AuthManager::revoke_client(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return false;
    }
    
    it->second.is_active = false;
    
    std::cout << "[AuthManager] Revoked client: " << client_id << std::endl;
    
    return true;
}

bool AuthManager::update_permissions(const std::string& client_id, const std::set<Permission>& new_permissions) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
        return false;
    }
    
    it->second.permissions = new_permissions;
    
    std::cout << "[AuthManager] Updated permissions for client: " << client_id << std::endl;
    
    return true;
}

std::vector<ClientInfo> AuthManager::get_all_clients() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ClientInfo> result;
    for (const auto& pair : clients_) {
        result.push_back(pair.second);
    }
    
    return result;
}

ClientInfo AuthManager::get_client_info(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return it->second;
    }
    
    ClientInfo empty_info;
    return empty_info;
}

void AuthManager::update_last_seen(const std::string& client_id, const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        it->second.last_seen = std::to_string(std::time(nullptr));
        it->second.ip_address = ip_address;
    }
}
