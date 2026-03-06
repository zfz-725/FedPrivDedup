#include "tls_connection_new.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

TLSConnection::TLSConnection()
    : socket_fd_(-1), is_server_(false), connected_(false),
      ssl_ctx_(nullptr), ssl_(nullptr) {
}

TLSConnection::~TLSConnection() {
    close();
}

bool TLSConnection::create_ssl_context() {
    const SSL_METHOD* method;
    
    if (is_server_) {
        method = TLS_server_method();
    } else {
        method = TLS_client_method();
    }
    
    ssl_ctx_ = SSL_CTX_new(method);
    if (!ssl_ctx_) {
        std::cerr << "[TLS] Failed to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_3_VERSION);
    
    return true;
}

bool TLSConnection::configure_ssl_context(const std::string& cert_path, const std::string& key_path) {
    if (!cert_path.empty()) {
        if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            std::cerr << "[TLS] Failed to load certificate: " << cert_path << std::endl;
            ERR_print_errors_fp(stderr);
            return false;
        }
    }
    
    if (!key_path.empty()) {
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            std::cerr << "[TLS] Failed to load private key: " << key_path << std::endl;
            ERR_print_errors_fp(stderr);
            return false;
        }
    }
    
    return true;
}

bool TLSConnection::init_client(const std::string& cert_path) {
    is_server_ = false;
    
    if (!create_ssl_context()) {
        return false;
    }
    
    if (!cert_path.empty()) {
        if (SSL_CTX_load_verify_locations(ssl_ctx_, cert_path.c_str(), nullptr) <= 0) {
            std::cerr << "[TLS] Failed to load CA certificate: " << cert_path << std::endl;
            ERR_print_errors_fp(stderr);
            return false;
        }
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
    } else {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    }
    
    std::cout << "[TLS] Client initialized with TLS 1.3" << std::endl;
    return true;
}

bool TLSConnection::init_server(const std::string& cert_path, const std::string& key_path) {
    is_server_ = true;
    
    if (!create_ssl_context()) {
        return false;
    }
    
    if (!configure_ssl_context(cert_path, key_path)) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
        return false;
    }
    
    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    
    std::cout << "[TLS] Server initialized with TLS 1.3" << std::endl;
    return true;
}

bool TLSConnection::connect(const std::string& host, int port) {
    if (is_server_) {
        std::cerr << "[TLS] Cannot connect in server mode" << std::endl;
        return false;
    }
    
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        std::cerr << "[TLS] Failed to resolve host: " << host << std::endl;
        return false;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[TLS] Failed to create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (::connect(socket_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[TLS] Failed to connect to " << host << ":" << port << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
        std::cerr << "[TLS] Failed to create SSL object" << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    SSL_set_fd(ssl_, socket_fd_);
    
    if (!perform_ssl_handshake()) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    std::cout << "[TLS] Connected to " << host << ":" << port << " with TLS 1.3" << std::endl;
    return true;
}

bool TLSConnection::bind_and_listen(int port, int max_connections) {
    if (!is_server_) {
        std::cerr << "[TLS] Cannot bind in client mode" << std::endl;
        return false;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[TLS] Failed to create socket" << std::endl;
        return false;
    }
    
    int opt = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    if (bind(socket_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[TLS] Failed to bind to port " << port << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    if (listen(socket_fd_, max_connections) < 0) {
        std::cerr << "[TLS] Failed to listen on port " << port << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    std::cout << "[TLS] Server listening on port " << port << " with TLS 1.3" << std::endl;
    return true;
}

int TLSConnection::accept_connection() {
    if (!is_server_) {
        std::cerr << "[TLS] Cannot accept in client mode" << std::endl;
        return -1;
    }
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(socket_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "[TLS] Failed to accept connection" << std::endl;
        return -1;
    }
    
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
        std::cerr << "[TLS] Failed to create SSL object for client" << std::endl;
        ::close(client_fd);
        return -1;
    }
    
    SSL_set_fd(ssl_, client_fd);
    
    if (SSL_accept(ssl_) <= 0) {
        std::cerr << "[TLS] Failed to perform SSL handshake" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_free(ssl_);
        ssl_ = nullptr;
        ::close(client_fd);
        return -1;
    }
    
    std::cout << "[TLS] Accepted TLS connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
    return client_fd;
}

bool TLSConnection::perform_ssl_handshake() {
    if (is_server_) {
        if (SSL_accept(ssl_) <= 0) {
            std::cerr << "[TLS] SSL accept failed" << std::endl;
            ERR_print_errors_fp(stderr);
            return false;
        }
    } else {
        if (SSL_connect(ssl_) <= 0) {
            std::cerr << "[TLS] SSL connect failed" << std::endl;
            ERR_print_errors_fp(stderr);
            return false;
        }
    }
    return true;
}

bool TLSConnection::send(const std::string& data) {
    return send(data.c_str(), data.length());
}

bool TLSConnection::send(const char* data, size_t length) {
    if (!connected_ || !ssl_) {
        std::cerr << "[TLS] Not connected" << std::endl;
        return false;
    }
    
    int result = SSL_write(ssl_, data, length);
    if (result <= 0) {
        int error = SSL_get_error(ssl_, result);
        std::cerr << "[TLS] Failed to send data, SSL error: " << error << std::endl;
        return false;
    }
    
    return true;
}

bool TLSConnection::receive(std::string& data, size_t max_length) {
    char buffer[max_length];
    int bytes_read;
    
    if (!receive(buffer, max_length, bytes_read)) {
        return false;
    }
    
    data = std::string(buffer, bytes_read);
    return true;
}

bool TLSConnection::receive(char* buffer, size_t buffer_size, int& bytes_read) {
    if (!connected_ || !ssl_) {
        std::cerr << "[TLS] Not connected" << std::endl;
        return false;
    }
    
    bytes_read = SSL_read(ssl_, buffer, buffer_size);
    if (bytes_read <= 0) {
        int error = SSL_get_error(ssl_, bytes_read);
        if (error == SSL_ERROR_ZERO_RETURN) {
            bytes_read = 0;
            return true;
        }
        std::cerr << "[TLS] Failed to receive data, SSL error: " << error << std::endl;
        return false;
    }
    
    return true;
}

void TLSConnection::close() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    
    connected_ = false;
}

bool TLSConnection::is_connected() const {
    return connected_;
}
