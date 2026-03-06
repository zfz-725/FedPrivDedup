#include "tls_connection.h"
#include <iostream>
#include <cstring>

TlsConnection::TlsConnection() : ctx_(nullptr), ssl_(nullptr), initialized_(false) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

TlsConnection::~TlsConnection() {
    cleanup();
}

bool TlsConnection::init_ssl_context() {
    const SSL_METHOD* method;
    
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    method = TLS_server_method();
#else
    method = TLSv1_2_server_method();
#endif
    
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        std::cerr << "[TLS] Failed to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);
    
    SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    
    return true;
}

bool TlsConnection::init_server(const std::string& cert_file, const std::string& key_file) {
    if (!init_ssl_context()) {
        return false;
    }
    
    if (SSL_CTX_use_certificate_file(ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "[TLS] Failed to load certificate" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "[TLS] Failed to load private key" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    if (!SSL_CTX_check_private_key(ctx_)) {
        std::cerr << "[TLS] Private key does not match certificate" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "[TLS] Server initialized with TLS 1.3" << std::endl;
    return true;
}

bool TlsConnection::init_client() {
    const SSL_METHOD* method;
    
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    method = TLS_client_method();
#else
    method = TLSv1_2_client_method();
#endif
    
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        std::cerr << "[TLS] Failed to create SSL context" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);
    
    SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    
    initialized_ = true;
    std::cout << "[TLS] Client initialized with TLS 1.3" << std::endl;
    return true;
}

SSL* TlsConnection::accept_connection(int socket_fd) {
    if (!initialized_) {
        std::cerr << "[TLS] Not initialized" << std::endl;
        return nullptr;
    }
    
    ssl_ = SSL_new(ctx_);
    SSL_set_fd(ssl_, socket_fd);
    
    if (SSL_accept(ssl_) <= 0) {
        std::cerr << "[TLS] SSL accept failed" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_free(ssl_);
        ssl_ = nullptr;
        return nullptr;
    }
    
    const char* version = SSL_get_version(ssl_);
    std::cout << "[TLS] Connection established using " << version << std::endl;
    
    return ssl_;
}

bool TlsConnection::connect_to_server(int socket_fd) {
    if (!initialized_) {
        std::cerr << "[TLS] Not initialized" << std::endl;
        return false;
    }
    
    ssl_ = SSL_new(ctx_);
    SSL_set_fd(ssl_, socket_fd);
    
    if (SSL_connect(ssl_) <= 0) {
        std::cerr << "[TLS] SSL connect failed" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_free(ssl_);
        ssl_ = nullptr;
        return false;
    }
    
    const char* version = SSL_get_version(ssl_);
    std::cout << "[TLS] Connected using " << version << std::endl;
    
    return true;
}

int TlsConnection::send(const std::string& data) {
    if (!ssl_) {
        std::cerr << "[TLS] SSL not initialized" << std::endl;
        return -1;
    }
    
    int result = SSL_write(ssl_, data.c_str(), data.length());
    if (result <= 0) {
        int error = SSL_get_error(ssl_, result);
        std::cerr << "[TLS] SSL write failed: " << error << std::endl;
        return -1;
    }
    
    return result;
}

int TlsConnection::receive(std::string& data, size_t max_size) {
    if (!ssl_) {
        std::cerr << "[TLS] SSL not initialized" << std::endl;
        return -1;
    }
    
    std::vector<char> buffer(max_size);
    int result = SSL_read(ssl_, buffer.data(), buffer.size());
    
    if (result <= 0) {
        int error = SSL_get_error(ssl_, result);
        if (error == SSL_ERROR_ZERO_RETURN) {
            return 0;
        }
        std::cerr << "[TLS] SSL read failed: " << error << std::endl;
        return -1;
    }
    
    data.assign(buffer.data(), result);
    return result;
}

void TlsConnection::close_connection() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
}

void TlsConnection::cleanup() {
    close_connection();
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}