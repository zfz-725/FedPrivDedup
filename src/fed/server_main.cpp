#include "server.h"
#include <iostream>

int main() {
    ServerConfig config;
    FedServer server(config);
    
    if (!server.init()) {
        std::cout << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    if (!server.start()) {
        std::cout << "Failed to start server" << std::endl;
        return 1;
    }
    
    return 0;
}