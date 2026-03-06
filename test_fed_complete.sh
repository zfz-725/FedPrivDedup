#!/bin/bash

# 联邦学习隐私保护去重框架测试脚本

set -e

echo "=========================================="
echo "联邦学习隐私保护去重框架测试"
echo "=========================================="

# 检查可执行文件是否存在
if [ ! -f "./fed_server" ]; then
    echo "错误: fed_server 可执行文件不存在"
    exit 1
fi

if [ ! -f "./fed_client" ]; then
    echo "错误: fed_client 可执行文件不存在"
    exit 1
fi

# 创建测试数据目录
mkdir -p test_data/client1
mkdir -p test_data/client2

# 生成测试数据
echo ""
echo "生成测试数据..."
cat > test_data/client1/test1.jsonl << 'EOF'
{"text": "This is a test document about machine learning and artificial intelligence."}
{"text": "Machine learning is a subset of artificial intelligence that focuses on algorithms."}
{"text": "Deep learning uses neural networks with multiple layers to learn patterns."}
{"text": "Natural language processing helps computers understand human language."}
{"text": "This is a test document about machine learning and artificial intelligence."}
{"text": "Computer vision enables machines to interpret visual information."}
{"text": "Reinforcement learning trains agents through reward systems."}
{"text": "Supervised learning uses labeled data for training models."}
{"text": "Unsupervised learning finds patterns in unlabeled data."}
{"text": "Transfer learning applies knowledge from one task to another."}
EOF

cat > test_data/client2/test1.jsonl << 'EOF'
{"text": "This is a test document about machine learning and artificial intelligence."}
{"text": "Machine learning algorithms can process large datasets efficiently."}
{"text": "Neural networks are inspired by biological brain structures."}
{"text": "Data preprocessing is crucial for machine learning success."}
{"text": "This is a test document about machine learning and artificial intelligence."}
{"text": "Feature engineering improves model performance significantly."}
{"text": "Cross-validation helps assess model generalization."}
{"text": "Hyperparameter tuning optimizes model configurations."}
{"text": "Ensemble methods combine multiple models for better predictions."}
{"text": "Gradient descent is an optimization algorithm for training."}
EOF

echo "测试数据生成完成"
echo ""

# 启动服务端
echo "启动服务端..."
./fed_server &
SERVER_PID=$!
echo "服务端 PID: $SERVER_PID"

# 等待服务端启动
sleep 3

# 测试服务端是否启动成功
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "错误: 服务端启动失败"
    exit 1
fi

echo "服务端启动成功"
echo ""

# 启动客户端1
echo "启动客户端1..."
./fed_client client1 org1 127.0.0.1 8080 test_data/client1 &
CLIENT1_PID=$!
echo "客户端1 PID: $CLIENT1_PID"

# 等待客户端1完成
wait $CLIENT1_PID
CLIENT1_EXIT_CODE=$?
echo "客户端1 退出代码: $CLIENT1_EXIT_CODE"

if [ $CLIENT1_EXIT_CODE -ne 0 ]; then
    echo "警告: 客户端1 执行失败"
fi

echo ""

# 启动客户端2
echo "启动客户端2..."
./fed_client client2 org2 127.0.0.1 8080 test_data/client2 &
CLIENT2_PID=$!
echo "客户端2 PID: $CLIENT2_PID"

# 等待客户端2完成
wait $CLIENT2_PID
CLIENT2_EXIT_CODE=$?
echo "客户端2 退出代码: $CLIENT2_EXIT_CODE"

if [ $CLIENT2_EXIT_CODE -ne 0 ]; then
    echo "警告: 客户端2 执行失败"
fi

echo ""

# 等待服务端处理完成
sleep 2

# 关闭服务端
echo "关闭服务端..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "=========================================="
echo "测试完成"
echo "=========================================="
echo ""
echo "测试总结:"
echo "- 服务端: 已启动并关闭"
echo "- 客户端1: 执行完成 (退出代码: $CLIENT1_EXIT_CODE)"
echo "- 客户端2: 执行完成 (退出代码: $CLIENT2_EXIT_CODE)"
echo ""
echo "请查看上面的日志输出以确认功能是否正常"
echo ""