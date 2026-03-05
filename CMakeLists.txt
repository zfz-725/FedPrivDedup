cmake_minimum_required(VERSION 3.10)
project(FED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 启用CUDA
find_package(CUDA REQUIRED)

# 添加包含目录
include_directories(src)

# CUDA库 - 包含crypto_cuda.cu用于GPU相似度计算
cuda_add_library(fed_cuda
    src/fed/crypto_cuda.cu
    STATIC
)

# 联邦学习模块
add_executable(fed_server src/fed/server_main.cpp src/fed/server.cpp src/fed/crypto.cpp)
add_executable(fed_client src/fed/client_main.cpp src/fed/client.cpp src/fed/crypto.cpp)

# 链接必要的库
target_link_libraries(fed_server pthread fed_cuda ${CUDA_LIBRARIES})
target_link_libraries(fed_client pthread fed_cuda ${CUDA_LIBRARIES})