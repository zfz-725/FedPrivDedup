cmake_minimum_required(VERSION 3.18)
project(FED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 启用CUDA
find_package(CUDAToolkit REQUIRED)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 添加包含目录
include_directories(src)

# 添加OpenSSL包含目录
include_directories(${OPENSSL_INCLUDE_DIR})

# CUDA库
add_library(fed_cuda STATIC
    src/fed/crypto_cuda.cu
    src/fed/fed_cuda.cu
    src/fed/decrypt_cuda.cu
)
set_target_properties(fed_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_ARCHITECTURES "70;75;80;86"
    LINKER_LANGUAGE CUDA
)
target_link_libraries(fed_cuda CUDA::cudart)

# 联邦学习模块
add_executable(fed_server 
    src/fed/server_main.cpp 
    src/fed/server.cpp 
    src/fed/crypto.cpp
    src/fed/aes_crypto.cpp
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
target_link_libraries(fed_server pthread fed_cuda CUDA::cudart OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(fed_client pthread fed_cuda CUDA::cudart OpenSSL::SSL OpenSSL::Crypto)