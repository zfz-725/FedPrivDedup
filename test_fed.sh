#!/bin/bash

# 测试脚本 - 联邦学习隐私保护去重框架

echo "========================================"
echo "联邦学习隐私保护去重框架测试"
echo "========================================"

# 清理之前的输出
rm -rf output
mkdir -p output

# 启动服务端
echo "[测试] 启动服务端..."
./fed_server &
SERVER_PID=$!

# 等待服务端启动
sleep 2

# 运行客户端1
echo "[测试] 运行客户端1 (org1)..."
./fed_client -i client1 -g org1 -d test/data 2>&1
CLIENT1_EXIT_CODE=$?

# 运行客户端2
echo "[测试] 运行客户端2 (org2)..."
./fed_client -i client2 -g org2 -d test/data 2>&1
CLIENT2_EXIT_CODE=$?

# 停止服务端
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "========================================"
echo "测试结果"
echo "========================================"
echo "客户端1退出码: $CLIENT1_EXIT_CODE"
echo "客户端2退出码: $CLIENT2_EXIT_CODE"
echo ""

# 显示去重结果
echo "[客户端1去重结果]"
if [ -f output/dedup_result_client1.txt ]; then
    cat output/dedup_result_client1.txt
else
    echo "无结果文件"
fi

echo ""
echo "[客户端2去重结果]"
if [ -f output/dedup_result_client2.txt ]; then
    cat output/dedup_result_client2.txt
else
    echo "无结果文件"
fi

echo ""
echo "========================================"
echo "测试完成"
echo "========================================"
