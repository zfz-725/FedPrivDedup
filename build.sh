#!/bin/bash

# 设置CUDA路径
CUDA_PATH=/root/miniconda3/envs/fed_env

# 创建build目录
mkdir -p build

# 编译CUDA库
nvcc -c -o build/crypto_cuda.o src/fed/crypto_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
nvcc -c -o build/fed_cuda.o src/fed/fed_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
nvcc -c -o build/decrypt_cuda.o src/fed/decrypt_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
ar rcs build/libfed_cuda.a build/crypto_cuda.o build/fed_cuda.o build/decrypt_cuda.o

# 编译服务器
g++ -o build/fed_server src/fed/server_main.cpp src/fed/server.cpp src/fed/crypto.cpp src/fed/aes_crypto.cpp src/fed/audit_logger.cpp src/fed/auth_manager.cpp -Lbuild -lfed_cuda -L$CUDA_PATH/lib -lcudart -lpthread -lssl -lcrypto -I$CUDA_PATH/include -Isrc

# 编译客户端
g++ -o build/fed_client src/fed/client_main.cpp src/fed/client.cpp src/fed/crypto.cpp src/fed/aes_crypto.cpp src/fed/union_find.cpp src/fed/data_preprocessor.cpp -Lbuild -lfed_cuda -L$CUDA_PATH/lib -lcudart -lpthread -lssl -lcrypto -I$CUDA_PATH/include -Isrc

echo "Compilation completed successfully!"