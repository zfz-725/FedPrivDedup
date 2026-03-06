cmake_minimum_required(VERSION 3.10)
project(FED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 启用CUDA
find_package(CUDA REQUIRED)

# 查找OpenSSL
find_package(OpenSSL)
if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found via find_package, trying pkg-config...")
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(OPENSSL openssl)
    endif()
endif()

# 添加包含目录
include_directories(src)

# 如果找到OpenSSL，添加包含目录
if(OPENSSL_FOUND OR OPENSSL_INCLUDE_DIR)
    if(OPENSSL_INCLUDE_DIR)
        include_directories(${OPENSSL_INCLUDE_DIR})
    endif()
    if(OPENSSL_INCLUDE_DIRS)
        include_directories(${OPENSSL_INCLUDE_DIRS})
    endif()
endif()

# CUDA库
cuda_add_library(fed_cuda
    src/fed/crypto_cuda.cu
    src/fed/fed_cuda.cu
    src/fed/decrypt_cuda.cu
    STATIC
)

# 联邦学习模块
add_executable(fed_server 
    src/fed/server_main.cpp 
    src/fed/server.cpp 
    src/fed/crypto.cpp
    src/fed/aes_crypto.cpp
    src/fed/tls_connection.cpp
    src/fed/tls_connection_new.cpp
    src/fed/audit_logger.cpp
    src/fed/auth_manager.cpp
)

add_executable(fed_client 
    src/fed/client_main.cpp 
    src/fed/client.cpp 
    src/fed/crypto.cpp
    src/fed/aes_crypto.cpp
    src/fed/union_find.cpp
    src/fed/data_preprocessor.cpp
)

# 链接必要的库
target_link_libraries(fed_server pthread fed_cuda ${CUDA_LIBRARIES})
target_link_libraries(fed_client pthread fed_cuda ${CUDA_LIBRARIES})

# 如果找到OpenSSL，链接OpenSSL库
if(OPENSSL_FOUND)
    target_link_libraries(fed_server OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(fed_client OpenSSL::SSL OpenSSL::Crypto)
elseif(OPENSSL_LIBRARIES)
    target_link_libraries(fed_server ${OPENSSL_LIBRARIES})
    target_link_libraries(fed_client ${OPENSSL_LIBRARIES})
else()
    # 手动链接OpenSSL
    target_link_libraries(fed_server /usr/lib/x86_64-linux-gnu/libssl.so.3 /usr/lib/x86_64-linux-gnu/libcrypto.so.3)
    target_link_libraries(fed_client /usr/lib/x86_64-linux-gnu/libssl.so.3 /usr/lib/x86_64-linux-gnu/libcrypto.so.3)
endif()