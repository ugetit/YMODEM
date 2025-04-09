# YMODEM 协议实现

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
**其他语言版本: [English](README-EN.md), [中文](README-CN.md).**
## 概述

这是一个用 C 语言实现的可移植 YMODEM 协议，专为嵌入式系统设计，受 [RT-Thread](https://www.rt-thread.org/) 启发。该实现高度可配置，依赖极少，同时支持文件发送和接收功能。

## 特性

- 完整的 YMODEM 协议实现，带 CRC16 错误检查
- 支持 128 字节和 1024 字节数据包
- 基于回调的设计，确保最大可移植性
- 最小 RAM 占用
- 支持文件发送和接收
- 跨平台兼容性
- 除标准 C 库外无外部依赖
- 全面的错误处理和超时管理

## 架构

YMODEM 实现分为三个主要组件：

1. **公共模块**：包含共享函数、定义和数据结构
2. **发送模块**：实现 YMODEM 文件发送功能
3. **接收模块**：实现 YMODEM 文件接收功能

## 目录结构

```
ymodem/
├── ymodem_common.c      # 公共工具函数
├── ymodem_common.h      # 公共定义和数据结构
├── ymodem_send.c        # 发送器实现
├── ymodem_send.h        # 发送器 API
├── ymodem_receive.c     # 接收器实现
├── ymodem_receive.h     # 接收器 API
├── examples/
│   ├── send_file.c      # 发送文件示例
│   └── rec_file.c       # 接收文件示例
└── README.md            # 本文件
```

## 使用方法

### 初始化回调函数

首先，你需要初始化回调函数来与硬件和文件系统接口：

```c
ymodem_callbacks_t callbacks = {
    // 文件操作
    .file_open = my_file_open,
    .file_read = my_file_read,
    .file_write = my_file_write,
    .file_close = my_file_close,
    .file_size = my_file_size,
    
    // 通信
    .comm_send = my_uart_send_byte,
    .comm_receive = my_uart_receive_byte,
    
    // 计时（可选）
    .get_time_ms = my_get_time_ms,
    .delay_ms = my_delay_ms
};
```

### 发送文件

```c
// 初始化 YMODEM 上下文
ymodem_context_t ctx;
uint8_t buffer[YMODEM_MAX_PACKET_SIZE];

// 初始化发送器
int ret = ymodem_send_init(&ctx, &callbacks, buffer, sizeof(buffer));
if (ret != YMODEM_ERR_NONE) {
    // 处理错误
}

// 发送文件
ret = ymodem_send_file(&ctx, "filename.bin", 10); // 10 秒握手超时
if (ret != YMODEM_ERR_NONE) {
    // 处理错误
}

// 清理资源
ymodem_send_cleanup(&ctx);
```

### 接收文件

```c
// 初始化 YMODEM 上下文
ymodem_context_t ctx;
uint8_t buffer[YMODEM_MAX_PACKET_SIZE];

// 初始化接收器
int ret = ymodem_receive_init(&ctx, &callbacks, buffer, sizeof(buffer));
if (ret != YMODEM_ERR_NONE) {
    // 处理错误
}

// 接收文件
ymodem_file_info_t file_info;
ret = ymodem_receive_file(&ctx, &file_info, 60); // 60 秒握手超时
if (ret != YMODEM_ERR_NONE) {
    // 处理错误
}

// 清理资源
ymodem_receive_cleanup(&ctx);

// 使用文件信息
printf("接收到文件: %s (%zu 字节)\n", file_info.filename, file_info.filesize);
```

## 配置

以下配置参数可以在构建系统或自定义头文件中定义：

```c
// 超时设置
#define YMODEM_WAIT_CHAR_TIMEOUT_MS     3000  // 字符接收超时（3秒）
#define YMODEM_WAIT_PACKET_TIMEOUT_MS   3000  // 数据包接收超时（3秒）
#define YMODEM_HANDSHAKE_INTERVAL_MS    1000  // 握手尝试间隔（1秒）

// 错误处理
#define YMODEM_MAX_ERRORS               5     // 中止前的最大错误次数
#define YMODEM_CAN_SEND_COUNT           7     // 取消传输时发送的 CAN 字节数
```

## 错误代码

该实现提供了详细的错误代码用于故障排除：

```c
YMODEM_ERR_NONE  =  0  // 无错误
YMODEM_ERR_TMO   = -1  // 握手超时
YMODEM_ERR_CODE  = -2  // 错误代码，错误的 SOH、STX 等
YMODEM_ERR_SEQ   = -3  // 错误的序列号
YMODEM_ERR_CRC   = -4  // 错误的 CRC 校验和
YMODEM_ERR_DSZ   = -5  // 接收数据不足
YMODEM_ERR_CAN   = -6  // 传输被用户中止
YMODEM_ERR_ACK   = -7  // 错误的应答，错误的 ACK 或 C
YMODEM_ERR_FILE  = -8  // 文件操作错误
YMODEM_ERR_MEM   = -9  // 内存分配错误
```

## 移植指南

要将此 YMODEM 实现移植到你的平台，你需要实现以下回调函数：

### 文件操作
- `file_open`：打开文件进行读取或写入
- `file_read`：从文件读取数据
- `file_write`：向文件写入数据
- `file_close`：关闭文件
- `file_size`：获取文件大小或剩余字节数

### 通信
- `comm_send`：发送单个字节
- `comm_receive`：带超时接收单个字节

### 计时（可选）
- `get_time_ms`：获取当前时间（毫秒）
- `delay_ms`：延时指定毫秒

示例文件中提供了标准 C 环境和 UART 通信的实现示例。

## 许可证

本项目基于 Apache License 2.0 许可证 - 详见 LICENSE 文件。

## 致谢

- 本实现受 [RT-Thread](https://www.rt-thread.org/) 中 YMODEM 实现的启发
- 感谢开源社区对 YMODEM 协议的标准化工作