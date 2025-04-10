/**
 * @file ymodem_common.c
 * @brief Common functions for portable YMODEM implementation
 * @date 2025-04-09
 * 
 * This file contains common utility functions for the YMODEM implementation.
 */

#include "ymodem_common.h"
#include <string.h>

/* CRC16 table for faster computation */
static const uint16_t crc16_ccitt_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

/**
 * @brief Convert YMODEM code to string representation for debugging
 */
const char* ymodem_code_to_str(enum ymodem_code code)
{
    switch (code) {
        case YMODEM_CODE_NONE: return "NONE";
        case YMODEM_CODE_SOH: return "SOH";
        case YMODEM_CODE_STX: return "STX";
        case YMODEM_CODE_EOT: return "EOT";
        case YMODEM_CODE_ACK: return "ACK";
        case YMODEM_CODE_NAK: return "NAK";
        case YMODEM_CODE_CAN: return "CAN";
        case YMODEM_CODE_C: return "C";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert YMODEM error code to string representation for debugging
 */
const char* ymodem_error_to_str(enum ymodem_error error)
{
    switch (error) {
        case YMODEM_ERR_NONE: return "NO_ERROR";
        case YMODEM_ERR_TMO: return "TIMEOUT";
        case YMODEM_ERR_CODE: return "WRONG_CODE";
        case YMODEM_ERR_SEQ: return "WRONG_SEQUENCE";
        case YMODEM_ERR_CRC: return "CRC_ERROR";
        case YMODEM_ERR_DSZ: return "DATA_SIZE_ERROR";
        case YMODEM_ERR_CAN: return "CANCELLED";
        case YMODEM_ERR_ACK: return "ACK_ERROR";
        case YMODEM_ERR_FILE: return "FILE_ERROR";
        case YMODEM_ERR_MEM: return "MEMORY_ERROR";
        default: return "UNKNOWN_ERROR";
    }
}

/**
 * @brief Convert YMODEM stage to string representation for debugging
 */
const char* ymodem_stage_to_str(enum ymodem_stage stage)
{
    switch (stage) {
        case YMODEM_STAGE_NONE: return "NONE";
        case YMODEM_STAGE_ESTABLISHING: return "ESTABLISHING";
        case YMODEM_STAGE_ESTABLISHED: return "ESTABLISHED";
        case YMODEM_STAGE_TRANSMITTING: return "TRANSMITTING";
        case YMODEM_STAGE_FINISHING: return "FINISHING";
        case YMODEM_STAGE_FINISHED: return "FINISHED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Calculate CRC16 CCITT for a buffer
 * 
 * @param buffer Buffer to calculate CRC for
 * @param size Size of the buffer
 * @return uint16_t Calculated CRC16
 */
uint16_t ymodem_calc_crc16(const uint8_t* buffer, size_t size)
{
    uint16_t crc = 0;
    
    while (size-- > 0) {
        crc = (crc << 8) ^ crc16_ccitt_table[((crc >> 8) ^ *buffer++) & 0xFF];
    }
    
    return crc;
}

/**
 * @brief Get basename from path
 * 
 * @param path Full file path
 * @return const char* Basename (last component of path)
 */
const char* ymodem_get_path_basename(const char* path)
{
    const char* basename = strrchr(path, '/');
    
    if (basename != NULL) {
        /* Skip the '/' character */
        return basename + 1;
    }
    
    /* No '/' found, try with backslash for Windows paths */
    basename = strrchr(path, '\\');
    if (basename != NULL) {
        /* Skip the '\' character */
        return basename + 1;
    }
    
    /* No path separator found, return the original path */
    return path;
}

/**
 * @brief Send bytes using the registered callback
 * 
 * @param ctx YMODEM context
 * @param data Buffer containing bytes to send
 * @param length Number of bytes to send
 * @return size_t Number of bytes sent, 0 on failure
 */
size_t ymodem_send_bytes(ymodem_context_t* ctx, const uint8_t* data, size_t length)
{
    if (ctx->callbacks.comm_send == NULL) {
        YMODEM_DEBUG_PRINT("Send failed: comm_send callback is NULL\n");
        return 0;
    }
    
    size_t sent = ctx->callbacks.comm_send(data, length);
    
    // 添加调试输出 - 只打印前几个字节避免大量输出
    if (sent > 0) {
        YMODEM_DEBUG_PRINT("Sent %zu bytes: ", sent);
        for (size_t i = 0; i < (sent > 8 ? 8 : sent); i++) {
            YMODEM_DEBUG_PRINT("%02X ", data[i]);
        }
        if (sent > 8) {
            YMODEM_DEBUG_PRINT("...");
        }
        YMODEM_DEBUG_PRINT("\n");
    } else {
        YMODEM_DEBUG_PRINT("Failed to send data (sent 0 bytes)\n");
    }
    
    return sent;
}

/**
 * @brief Send a single byte using the registered callback
 * 
 * @param ctx YMODEM context
 * @param data Byte to send
 * @return bool true if successful, false otherwise
 */
bool ymodem_send_byte(ymodem_context_t* ctx, uint8_t data)
{
    bool result = (ymodem_send_bytes(ctx, &data, 1) == 1);
    if (result) {
        if (data >= 32 && data <= 126) {
            YMODEM_DEBUG_PRINT("Sent byte: 0x%02X ('%c') [%s]\n", data, data, ymodem_code_to_str(data));
        } else {
            YMODEM_DEBUG_PRINT("Sent byte: 0x%02X [%s]\n", data, ymodem_code_to_str(data));
        }
    } else {
        YMODEM_DEBUG_PRINT("Failed to send byte: 0x%02X\n", data);
    }
    return result;
}

/**
 * @brief Receive bytes using the registered callback
 * 
 * @param ctx YMODEM context
 * @param data Buffer to store received bytes
 * @param length Maximum number of bytes to receive
 * @param timeout_ms Timeout in milliseconds
 * @return size_t Number of bytes received, 0 on timeout or error
 */
size_t ymodem_receive_bytes(ymodem_context_t* ctx, uint8_t* data, size_t length, uint32_t timeout_ms)
{
    if (ctx->callbacks.comm_receive == NULL) {
        YMODEM_DEBUG_PRINT("Receive failed: comm_receive callback is NULL\n");
        return 0;
    }
    
    YMODEM_DEBUG_PRINT("Waiting to receive up to %zu bytes (timeout %u ms)...\n", length, timeout_ms);
    size_t received = ctx->callbacks.comm_receive(data, length, timeout_ms);
    
    if (received > 0) {
        YMODEM_DEBUG_PRINT("Received %zu bytes: ", received);
        for (size_t i = 0; i < (received > 8 ? 8 : received); i++) {
            YMODEM_DEBUG_PRINT("%02X ", data[i]);
        }
        if (received > 8) {
            YMODEM_DEBUG_PRINT("...");
        }
        YMODEM_DEBUG_PRINT("\n");
    } else {
        YMODEM_DEBUG_PRINT("Receive timeout or error (received 0 bytes)\n");
    }
    
    return received;
}

/**
 * @brief Receive a single byte using the registered callback
 * 
 * @param ctx YMODEM context
 * @param timeout_ms Timeout in milliseconds
 * @return int Received byte (0-255) or negative error code on failure
 */
int ymodem_receive_byte(ymodem_context_t* ctx, uint32_t timeout_ms)
{
    uint8_t data;
    size_t received;
    
    if (ctx->callbacks.comm_receive == NULL) {
        YMODEM_DEBUG_PRINT("Receive byte failed: comm_receive callback is NULL\n");
        return YMODEM_ERR_CODE;
    }
    
    YMODEM_DEBUG_PRINT("Waiting for single byte (timeout %u ms)...\n", timeout_ms);
    received = ymodem_receive_bytes(ctx, &data, 1, timeout_ms);
    if (received == 0) {
        YMODEM_DEBUG_PRINT("Byte receive timeout\n");
        return YMODEM_ERR_TMO;  /* Timeout */
    }
    
    if (data >= 32 && data <= 126) {
        YMODEM_DEBUG_PRINT("Received byte: 0x%02X ('%c') [%s]\n", data, data, ymodem_code_to_str(data));
    } else {
        YMODEM_DEBUG_PRINT("Received byte: 0x%02X [%s]\n", data, ymodem_code_to_str(data));
    }
    
    return data;
}