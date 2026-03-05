#include "client.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cout << "Usage: " << argv[0] << " <client_id> <org_id> <server_address> <server_port> <data_dir>" << std::endl;
        return 1;
    }
    
    ClientConfig config;
    config.client_id = argv[1];
    config.org_id = argv[2];
    config.server_address = argv[3];
    config.server_port = std::stoi(argv[4]);
    config.data_dir = argv[5];
    config.output_dir = "./output_" + config.client_id;
    
    FedClient client(config);
    
    if (!client.run()) {
        std::cout << "Client run failed" << std::endl;
        return 1;
    }
    
    std::cout << "Client run successful" << std::endl;
    return 0;
}