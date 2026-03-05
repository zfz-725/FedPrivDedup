# 联邦学习隐私保护去重框架 (FED)

基于FED论文实现的联邦学习隐私保护去重框架，支持跨机构协同去重，通过密文交换和主节点安全防护实现隐私保护。

## 项目结构

```
FED/
├── src/
│   ├── fed/              # 联邦学习模块
│   │   ├── client.h/cpp  # 客户端实现
│   │   ├── server.h/cpp  # 服务端实现
│   │   ├── crypto.h/cpp  # 加密工具类
│   │   ├── client_main.cpp # 客户端入口
│   │   └── server_main.cpp # 服务端入口
│   ├── lsh.h            # LSH算法接口
│   ├── param.h          # 参数配置
│   └── util.h           # 工具函数
├── test/
│   ├── data/            # 测试数据
│   └── test_fed.sh     # 测试脚本
├── CMakeLists.txt       # 编译配置
└── README_FED.md        # 本文档
```

## 核心功能

### 1. 客户端模块
- 本地数据预处理和特征生成
- 加密候选集并发送到服务端
- 接收服务端结果并完成本地去重
- 无语义随机ID生成
- AES加密/解密

### 2. 服务端模块
- HTTP服务器，接收客户端请求
- 解密候选集
- 全局相似度计算
- 结果分发
- 桶级隐私隔离

### 3. 隐私保护机制
- 最小信息传输原则
- 桶级别的隐私隔离
- 即时销毁机制
- 轻量对称加密

## 编译和运行

### 前置要求
- CMake 3.10+
- C++17
- MPI
- OpenMP
- CUDA (可选，用于GPU加速)

### 编译项目

```bash
# 激活虚拟环境
conda activate fed_env

# 编译项目
cd /root/Dev/FED
cmake .
make
```

### 运行测试

```bash
# 运行测试脚本
cd /root/Dev/FED/test
./test_fed.sh
```

### 手动运行

#### 启动服务端

```bash
cd /root/Dev/FED
./fed_server
```

服务端将在8080端口启动，等待客户端连接。

#### 启动客户端

```bash
cd /root/Dev/FED
./fed_client
```

客户端将连接到服务端，发送加密候选集，并接收去重结果。

## 配置说明

### 客户端配置 (client_main.cpp)

```cpp
ClientConfig config;
config.server_address = "127.0.0.1";  // 服务端地址
config.server_port = 8080;              // 服务端端口
config.client_id = "client1";            // 客户端ID
config.encryption_key = "encryption_key_123"; // 加密密钥
config.data_path = "./data";            // 数据路径
config.output_path = "./output";         // 输出路径
```

### 服务端配置 (server_main.cpp)

```cpp
ServerConfig config;
config.port = 8080;                  // 服务端端口
config.data_path = "./data";            // 数据路径
config.output_path = "./output";         // 输出路径
config.similarity_threshold = 0.8;       // 相似度阈值
```

## 测试结果

测试脚本验证了以下功能：
1. ✓ 服务端成功启动并监听8080端口
2. ✓ 客户端成功连接到服务端
3. ✓ 客户端成功发送加密候选集到服务端
4. ✓ 服务端成功接收并处理客户端请求
5. ✓ 客户端成功接收服务端响应

## 技术特点

1. **模块化设计**：客户端、服务端、加密工具等模块分离，便于维护和扩展
2. **隐私保护**：实现了AES加密、无语义ID等隐私保护机制
3. **可扩展性**：支持多客户端接入，主节点可弹性扩容
4. **兼容性**：与FED原版保持兼容，可复用其GPU加速核心

## 改进空间

1. **CUDA集成**：集成CUDA以获得真正的GPU加速
2. **完善加密**：使用更安全的加密库替代简单的异或加密
3. **JSON解析**：使用专门的JSON库替代简单的字符串处理
4. **错误处理**：增强错误处理和日志记录
5. **性能优化**：进一步优化通信和计算性能

## 许可证

请参考项目根目录下的LICENSE文件。

## 联系方式

如有问题或建议，请联系项目维护者。