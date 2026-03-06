#ifndef TLS_CONNECTION_H
#define TLS_CONNECTION_H

#include <string>
#include <vector>

// OpenSSL前向声明
struct ssl_ctx_st;
struct ssl_st;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

class TLSConnection {
public:
    TLSConnection();
    ~TLSConnection();
    
    bool init_client(const std::string& cert_path = "");
    bool init_server(const std::string& cert_path, const std::string& key_path);
    
    bool connect(const std::string& host, int port);
    bool bind_and_listen(int port, int max_connections = 10);
    
    int accept_connection();
    
    bool send(const std::string& data);
    bool send(const char* data, size_t length);
    
    bool receive(std::string& data, size_t max_length = 4096);
    bool receive(char* buffer, size_t buffer_size, int& bytes_read);
    
    void close();
    
    bool is_connected() const;
    
private:
    int socket_fd_;
    bool is_server_;
    bool connected_;
    
    SSL_CTX* ssl_ctx_;
    SSL* ssl_;
    
    bool create_ssl_context();
    bool configure_ssl_context(const std::string& cert_path, const std::string& key_path);
    bool perform_ssl_handshake();
};

#endif // TLS_CONNECTION_H
