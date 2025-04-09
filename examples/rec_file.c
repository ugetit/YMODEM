#include "ymodem_receive.h"
#include <stdio.h>
#include <stdlib.h>

// 文件操作回调实现
void* my_file_open(const char* filename, bool writing) {
    return fopen(filename, writing ? "wb" : "rb");
}

size_t my_file_read(void* file_handle, uint8_t* buffer, size_t size) {
    return fread(buffer, 1, size, (FILE*)file_handle);
}

size_t my_file_write(void* file_handle, const uint8_t* buffer, size_t size) {
    return fwrite(buffer, 1, size, (FILE*)file_handle);
}

void my_file_close(void* file_handle) {
    fclose((FILE*)file_handle);
}

int my_file_size(void* file_handle) {
    FILE* fp = (FILE*)file_handle;
    long current = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, current, SEEK_SET);
    return (int)size;
}

// 通信回调实现（示例使用UART）
bool my_uart_send_byte(uint8_t data) {
    // 使用你的UART发送函数
    return uart_send_byte(data) == 0; // 假设返回0表示成功
}

int my_uart_receive_byte(uint8_t* data, uint32_t timeout_ms) {
    // 使用你的UART接收函数（带超时）
    int ret = uart_receive_byte_timeout(data, timeout_ms);
    if (ret == 0) return 0; // 成功
    if (ret == -1) return YMODEM_ERR_TMO; // 超时
    return YMODEM_ERR_CODE; // 其他错误
}

// 时间回调实现
uint32_t my_get_time_ms(void) {
    // 返回当前系统时间（毫秒）
    return get_system_time_ms();
}

void my_delay_ms(uint32_t ms) {
    // 延时指定毫秒
    delay_ms(ms);
}

int receive_file_example(void) {
    // 注册回调函数
    ymodem_callbacks_t callbacks = {
        .file_open = my_file_open,
        .file_read = my_file_read,
        .file_write = my_file_write,
        .file_close = my_file_close,
        .file_size = my_file_size,
        .comm_send = my_uart_send_byte,
        .comm_receive = my_uart_receive_byte,
        .get_time_ms = my_get_time_ms,
        .delay_ms = my_delay_ms
    };
    
    // 分配缓冲区
    uint8_t* buffer = malloc(YMODEM_MAX_PACKET_SIZE);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return -1;
    }
    
    // 初始化YMODEM上下文
    ymodem_context_t ctx;
    int ret = ymodem_receive_init(&ctx, &callbacks, buffer, YMODEM_MAX_PACKET_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        printf("Failed to initialize YMODEM: %d\n", ret);
        free(buffer);
        return -1;
    }
    
    // 接收文件
    ymodem_file_info_t file_info;
    printf("Waiting to receive file...\n");
    ret = ymodem_receive_file(&ctx, &file_info, 60); // 60秒握手超时
    
    // 清理资源
    ymodem_receive_cleanup(&ctx);
    free(buffer);
    
    if (ret == YMODEM_ERR_NONE) {
        printf("File received successfully: %s (%zu bytes)\n", 
               file_info.filename, file_info.filesize);
        return 0;
    } else {
        printf("Failed to receive file, error: %d\n", ret);
        return -1;
    }
}