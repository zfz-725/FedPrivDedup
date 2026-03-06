#!/bin/bash

# 设置CUDA路径
CUDA_PATH=/root/miniconda3/envs/fed_env

# 编译CUDA库
nvcc -c -o crypto_cuda.o src/fed/crypto_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
nvcc -c -o fed_cuda.o src/fed/fed_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
nvcc -c -o decrypt_cuda.o src/fed/decrypt_cuda.cu -arch=sm_70 -I$CUDA_PATH/include
ar rcs libfed_cuda.a crypto_cuda.o fed_cuda.o decrypt_cuda.o

# 编译服务器
g++ -o fed_server src/fed/server_main.cpp src/fed/server.cpp src/fed/crypto.cpp src/fed/aes_crypto.cpp src/fed/audit_logger.cpp src/fed/auth_manager.cpp -L. -lfed_cuda -L$CUDA_PATH/lib -lcudart -lpthread -lssl -lcrypto -I$CUDA_PATH/include -Isrc

# 编译客户端
g++ -o fed_client src/fed/client_main.cpp src/fed/client.cpp src/fed/crypto.cpp src/fed/aes_crypto.cpp src/fed/union_find.cpp src/fed/data_preprocessor.cpp -L. -lfed_cuda -L$CUDA_PATH/lib -lcudart -lpthread -lssl -lcrypto -I$CUDA_PATH/include -Isrc

echo "Compilation completed successfully!"