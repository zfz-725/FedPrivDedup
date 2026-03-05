#include "client.h"
#include <iostream>

int main() {
    ClientConfig config;
    config.client_id = "client1";
    config.server_address = "127.0.0.1";
    config.server_port = 8080;
    config.org_id = "org1";
    
    FedClient client(config);
    
    if (!client.run()) {
        std::cout << "Client run failed" << std::endl;
        return 1;
    }
    
    std::cout << "Client run successful" << std::endl;
    return 0;
}