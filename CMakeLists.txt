cmake_minimum_required(VERSION 3.10)
project(FED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加包含目录
include_directories(src)

# 联邦学习模块
add_executable(fed_server src/fed/server_main.cpp src/fed/server.cpp src/fed/crypto.cpp)
add_executable(fed_client src/fed/client_main.cpp src/fed/client.cpp src/fed/crypto.cpp)

# 链接必要的库
target_link_libraries(fed_server pthread)
target_link_libraries(fed_client pthread)