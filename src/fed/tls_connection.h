#ifndef TLS_CONNECTION_H
#define TLS_CONNECTION_H

#include <string>
#include <vector>
#include <openssl/ssl.h>
#include <openssl/err.h>

class TlsConnection {
public:
    TlsConnection();
    ~TlsConnection();
    
    bool init_server(const std::string& cert_file, const std::string& key_file);
    bool init_client();
    
    SSL* accept_connection(int socket_fd);
    bool connect_to_server(int socket_fd);
    
    int send(const std::string& data);
    int receive(std::string& data, size_t max_size = 8192);
    
    void close_connection();
    
    SSL* get_ssl() const { return ssl_; }
    
private:
    SSL_CTX* ctx_;
    SSL* ssl_;
    bool initialized_;
    
    bool init_ssl_context();
    void cleanup();
};

#endif // TLS_CONNECTION_H