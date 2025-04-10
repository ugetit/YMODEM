#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <string.h>  /* For memcpy, strcpy, strcmp */
#include "ymodem_common.h"
#include "ymodem_send.h"
#include "ymodem_receive.h"
#include <sys/select.h>

// 文件操作回调
void* file_open_callback(const char* filename, bool writing) {
    FILE* file;
    if (writing) {
        file = fopen(filename, "wb");
    } else {
        file = fopen(filename, "rb");
    }
    return file;
}

size_t file_read_callback(void* file_handle, uint8_t* buffer, size_t size) {
    return fread(buffer, 1, size, (FILE*)file_handle);
}

size_t file_write_callback(void* file_handle, const uint8_t* buffer, size_t size) {
    return fwrite(buffer, 1, size, (FILE*)file_handle);
}

void file_close_callback(void* file_handle) {
    fclose((FILE*)file_handle);
}

int file_size_callback(void* file_handle) {
    FILE* file = (FILE*)file_handle;
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, current_pos, SEEK_SET);
    return (int)size;
}

// 串口通信回调
int serial_fd = -1;

size_t comm_send_callback(const uint8_t* data, size_t length) {
    return write(serial_fd, data, length);
}

// 时间处理回调
uint32_t get_time_ms_callback(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

size_t comm_receive_callback(uint8_t* data, size_t max_length, uint32_t timeout_ms) {
    // 在阻塞模式下，我们可以使用select来实现超时
    fd_set fds;
    struct timeval tv;
    size_t total_received = 0;
    
    while (total_received < max_length) {
        FD_ZERO(&fds);
        FD_SET(serial_fd, &fds);
        
        // 设置select超时
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        int ret = select(serial_fd + 1, &fds, NULL, NULL, &tv);
        
        if (ret <= 0) {
            // 超时或错误
            break;
        }
        
        // 阻塞读取，但有select超时保护
        ssize_t bytes_read = read(serial_fd, data + total_received, max_length - total_received);
        
        if (bytes_read <= 0) {
            // 读取错误
            break;
        }
        
        total_received += bytes_read;
        
        // 如果我们只是想读取任何可用数据，可以在这里立即返回
        // 如果我们需要满足特定长度的读取，保持循环直到达到长度或超时
        // 根据YMODEM实现的需要决定策略
        
        // 简单优化：如果只请求一个字节，并且我们已经读取了一个字节，直接返回
        if (max_length == 1 && total_received == 1) {
            break;
        }
    }
    
    return total_received;
}

void delay_ms_callback(uint32_t ms) {
    usleep(ms * 1000);
}

// 打开串口
int open_serial_port(const char* port) {
    // 移除O_NONBLOCK标志
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("open_serial_port: Unable to open port");
        return -1;
    }
    
    struct termios options;
    tcgetattr(fd, &options);
    
    // 设置波特率
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    // 设置为原始模式
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    // 设置阻塞读取的超时
    // 这里设置了一个非零的VMIN，表示至少要读取一个字符
    // 并设置了VTIME为超时时间（单位为0.1秒）
    options.c_cc[VMIN] = 1;    // 至少读取1个字符
    options.c_cc[VTIME] = 1;   // 0.1秒超时
    
    tcsetattr(fd, TCSANOW, &options);
    
    return fd;
}

int ymodem_send_test(const char* serial_port, const char* filename) {
    // 打开串口
    serial_fd = open_serial_port(serial_port);
    if (serial_fd < 0) {
        printf("Failed to open serial port %s\n", serial_port);
        return -1;
    }
    
    // 初始化回调函数
    ymodem_callbacks_t callbacks = {
        .file_open = file_open_callback,
        .file_read = file_read_callback,
        .file_write = file_write_callback,
        .file_close = file_close_callback,
        .file_size = file_size_callback,
        .comm_send = comm_send_callback,
        .comm_receive = comm_receive_callback,
        .get_time_ms = get_time_ms_callback,
        .delay_ms = delay_ms_callback
    };
    
    // 分配缓冲区
    uint8_t* buffer = (uint8_t*)malloc(YMODEM_MAX_PACKET_SIZE);
    uint8_t* send_buffer = (uint8_t*)malloc(YMODEM_MAX_PACKET_SIZE);
    
    // 初始化YMODEM上下文
    ymodem_context_t ctx;
    int ret = ymodem_send_init(&ctx, &callbacks, buffer, YMODEM_MAX_PACKET_SIZE, 
                               send_buffer, YMODEM_MAX_PACKET_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        printf("Failed to initialize YMODEM context: %d\n", ret);
        close(serial_fd);
        free(buffer);
        free(send_buffer);
        return -1;
    }
    
    // 发送文件
    printf("Sending file %s...\n", filename);
    ret = ymodem_send_file(&ctx, filename, 10); // 10秒握手超时
    
    if (ret == YMODEM_ERR_NONE) {
        printf("File sent successfully.\n");
    } else {
        printf("Failed to send file: %d\n", ret);
    }
    
    // 清理资源
    ymodem_send_cleanup(&ctx);
    close(serial_fd);
    free(buffer);
    free(send_buffer);
    
    return ret;
}

int ymodem_receive_test(const char* serial_port, const char* save_path) {
    // 打开串口
    serial_fd = open_serial_port(serial_port);
    if (serial_fd < 0) {
        printf("Failed to open serial port %s\n", serial_port);
        return -1;
    }
    
    // 初始化回调函数
    ymodem_callbacks_t callbacks = {
        .file_open = file_open_callback,
        .file_read = file_read_callback,
        .file_write = file_write_callback,
        .file_close = file_close_callback,
        .file_size = file_size_callback,
        .comm_send = comm_send_callback,
        .comm_receive = comm_receive_callback,
        .get_time_ms = get_time_ms_callback,
        .delay_ms = delay_ms_callback
    };
    
    // 分配缓冲区
    uint8_t* buffer = (uint8_t*)malloc(YMODEM_MAX_PACKET_SIZE);
    
    // 初始化YMODEM上下文
    ymodem_context_t ctx;
    int ret = ymodem_receive_init(&ctx, &callbacks, buffer, YMODEM_MAX_PACKET_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        printf("Failed to initialize YMODEM context: %d\n", ret);
        close(serial_fd);
        free(buffer);
        return -1;
    }
    
    // 准备接收文件
    ymodem_file_info_t file_info;
    
    // 如果save_path是目录，则在其中保存文件
    // 否则直接使用save_path作为文件路径
    char save_dir[256] = {0};
    strcpy(save_dir, save_path);
    
    printf("Waiting to receive file...\n");
    ret = ymodem_receive_file(&ctx, &file_info, 60); // 60秒握手超时
    
    if (ret == YMODEM_ERR_NONE) {
        printf("File received successfully: %s, size: %lu bytes\n", 
               file_info.filename, file_info.filesize);
    } else {
        printf("Failed to receive file: %d\n", ret);
    }
    
    // 清理资源
    ymodem_receive_cleanup(&ctx);
    close(serial_fd);
    free(buffer);
    
    return ret;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  Send file: %s send <serial_port> <file_to_send>\n", argv[0]);
        printf("  Receive file: %s receive <serial_port> <save_directory>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "send") == 0 && argc >= 4) {
        return ymodem_send_test(argv[2], argv[3]);
    } 
    else if (strcmp(argv[1], "receive") == 0 && argc >= 4) {
        return ymodem_receive_test(argv[2], argv[3]);
    } 
    else {
        printf("Invalid command\n");
        return 1;
    }
}