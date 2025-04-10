/**
 * @file ymodem_send.c
 * @brief YMODEM sender implementation
 * @date 2025-04-09
 * 
 * This file contains the implementation of the YMODEM sender.
 */

#include "ymodem_send.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations of internal functions */
static int _ymodem_do_send_handshake(ymodem_context_t* ctx, int timeout_s);
static int _ymodem_send_packet(ymodem_context_t* ctx, enum ymodem_code code, uint8_t seq, const uint8_t* data, size_t data_size);
static int _ymodem_prepare_file_info_packet(ymodem_context_t* ctx, const char* filename);
static int _ymodem_do_send_trans(ymodem_context_t* ctx);
static int _ymodem_do_send_fin(ymodem_context_t* ctx);

/**
 * @brief Initialize YMODEM context for sending
 */
int ymodem_send_init(ymodem_context_t* ctx, 
                    const ymodem_callbacks_t* callbacks,
                    uint8_t* buffer,
                    size_t buffer_size,
                    uint8_t* send_buffer,
                    size_t send_buffer_size)
{
    if (ctx == NULL || callbacks == NULL || buffer == NULL || send_buffer == NULL) {
        return YMODEM_ERR_CODE;
    }
    
    if (buffer_size < YMODEM_MAX_PACKET_SIZE || send_buffer_size < YMODEM_MAX_PACKET_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    /* Check required callbacks */
    if (callbacks->comm_send == NULL || 
        callbacks->comm_receive == NULL || 
        callbacks->file_open == NULL || 
        callbacks->file_read == NULL || 
        callbacks->file_close == NULL || 
        callbacks->file_size == NULL) {
        return YMODEM_ERR_CODE;
    }
    
    /* Initialize context */
    ctx->callbacks = *callbacks;
    ctx->buffer = buffer;
    ctx->buffer_size = buffer_size;
    ctx->send_buffer = send_buffer;
    ctx->send_buffer_size = send_buffer_size;
    ctx->stage = YMODEM_STAGE_NONE;
    ctx->file_handle = NULL;
    ctx->file_size = 0;
    ctx->packet_seq = 0;
    ctx->error_count = 0;
    ctx->filename[0] = '\0';
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Send a file via YMODEM protocol
 */
int ymodem_send_file(ymodem_context_t* ctx, 
                    const char* filename,
                    int handshake_timeout_s)
{
    int ret;
    
    if (ctx == NULL || filename == NULL) {
        return YMODEM_ERR_CODE;
    }
    
    /* Open file for reading */
    ctx->file_handle = ctx->callbacks.file_open(filename, false);
    if (ctx->file_handle == NULL) {
        return YMODEM_ERR_FILE;
    }
    
    /* Get file size */
    ctx->file_size = ctx->callbacks.file_size(ctx->file_handle);
    if (ctx->file_size < 0) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
        return YMODEM_ERR_FILE;
    }
    
    /* Save filename */
    const char* basename = ymodem_get_path_basename(filename);
    strncpy(ctx->filename, basename, YMODEM_MAX_FILENAME_LENGTH - 1);
    ctx->filename[YMODEM_MAX_FILENAME_LENGTH - 1] = '\0';
    
    /* Start handshake */
    ret = _ymodem_do_send_handshake(ctx, handshake_timeout_s);
    if (ret != YMODEM_ERR_NONE) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
        return ret;
    }

    YMODEM_DEBUG_PRINT("Handshake completed, starting file transfer\n"); 
    /* Send file data */
    ret = _ymodem_do_send_trans(ctx);
    if (ret != YMODEM_ERR_NONE) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
        return ret;
    }
    YMODEM_DEBUG_PRINT("Starting transmission finish sequence\n");
    
    /* Finish transmission */
    ret = _ymodem_do_send_fin(ctx);
    
    /* Close file */
    ctx->callbacks.file_close(ctx->file_handle);
    ctx->file_handle = NULL;
    YMODEM_DEBUG_PRINT("Transmission successfully completed\n");
    return ret;
}

/**
 * @brief Clean up YMODEM send resources
 */
void ymodem_send_cleanup(ymodem_context_t* ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    /* If file is still open, close it */
    if (ctx->file_handle != NULL) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
    }
    
    ctx->stage = YMODEM_STAGE_NONE;
}

/**
 * @brief Perform YMODEM sender handshake
 * 
 * Wait for 'C' character and send file info packet.
 */
// static int _ymodem_do_send_handshake(ymodem_context_t* ctx, int timeout_s)
// {
//     int i;
//     int ret;
//     YMODEM_DEBUG_PRINT("Starting handshake, waiting for 'C' (timeout: %d seconds)...\n", timeout_s);
//     ctx->stage = YMODEM_STAGE_ESTABLISHING;
    
//     /* Wait for 'C' character to start transfer */
//     for (i = 0; i < timeout_s; i++) {
//         ret = ymodem_receive_byte(ctx, YMODEM_HANDSHAKE_INTERVAL_MS);
//         if (ret == YMODEM_CODE_C) {
//             YMODEM_DEBUG_PRINT("Received 'C', sending file info packet for '%s'...\n", ctx->filename);
//             break;
//         }
//     }
    
//     if (i == timeout_s) {
//         return YMODEM_ERR_TMO;
//     }
    
//     /* Prepare and send file info packet (packet 0) */
//     ret = _ymodem_prepare_file_info_packet(ctx, ctx->filename);
//     if (ret != YMODEM_ERR_NONE) {
//         return ret;
//     }
    
//     /* Send the file info packet */
//     ret = _ymodem_send_packet(ctx, YMODEM_CODE_SOH, 0, ctx->buffer + 3, YMODEM_SOH_DATA_SIZE);
//     if (ret != YMODEM_ERR_NONE) {
//         return ret;
//     }
//     YMODEM_DEBUG_PRINT("File info packet sent, file size: %d bytes\n", ctx->file_size);
    
//     /* Wait for ACK */
//     ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//     if (ret != YMODEM_CODE_ACK) {
//         return YMODEM_ERR_ACK;
//     }
//     YMODEM_DEBUG_PRINT("File info ACKed, waiting for second 'C'...\n");
    
//     /* Wait for 'C' */
//     ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//     if (ret != YMODEM_CODE_C) {
//         return YMODEM_ERR_ACK;
//     }
    
//     ctx->stage = YMODEM_STAGE_ESTABLISHED;
//     ctx->packet_seq = 1; /* Start with packet 1 for actual data */
    
//     return YMODEM_ERR_NONE;
// }
static int _ymodem_do_send_handshake(ymodem_context_t* ctx, int timeout_s)
{
    int i;
    int ret;
    YMODEM_DEBUG_PRINT("Starting handshake, waiting for 'C' (timeout: %d seconds)...\n", timeout_s);
    ctx->stage = YMODEM_STAGE_ESTABLISHING;
    
    /* Wait for 'C' character to start transfer */
    for (i = 0; i < timeout_s; i++) {
        ret = ymodem_receive_byte(ctx, YMODEM_HANDSHAKE_INTERVAL_MS);
        if (ret == YMODEM_CODE_C) {
            YMODEM_DEBUG_PRINT("Received 'C', sending file info packet for '%s'...\n", ctx->filename);
            break;
        }
    }
    
    if (i == timeout_s) {
        return YMODEM_ERR_TMO;
    }
    
    /* Prepare and send file info packet (packet 0) */
    ret = _ymodem_prepare_file_info_packet(ctx, ctx->filename);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* Send the file info packet */
    ret = _ymodem_send_packet(ctx, YMODEM_CODE_SOH, 0, ctx->buffer + 3, YMODEM_SOH_DATA_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    YMODEM_DEBUG_PRINT("File info packet sent, file size: %d bytes\n", ctx->file_size);
    
    /* Wait for ACK and/or C with multiple attempts - modified to be more flexible */
    bool got_ack = false;
    bool got_c = false;
    
    // Try to receive both ACK and C in any order, with multiple attempts
    for (i = 0; i < 5; i++) {  // Try up to 5 times
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        
        if (ret == YMODEM_CODE_ACK) {
            YMODEM_DEBUG_PRINT("Received ACK for file info packet\n");
            got_ack = true;
        } 
        else if (ret == YMODEM_CODE_C) {
            YMODEM_DEBUG_PRINT("Received 'C' to start data transfer\n");
            got_c = true;
        }
        
        // If we've got both signals we need, we can proceed
        if (got_ack && got_c) {
            break;
        }
        
        // If we got ACK but not C, wait for C
        if (got_ack && !got_c) {
            continue;
        }
        
        // If we got C but not ACK, we might have missed the ACK but can proceed anyway
        if (!got_ack && got_c) {
            YMODEM_DEBUG_PRINT("Got C without ACK, assuming ACK was sent and proceeding\n");
            got_ack = true;
            break;
        }
    }
    
    if (!got_ack || !got_c) {
        YMODEM_DEBUG_PRINT("Handshake failed: ACK=%d, C=%d\n", got_ack, got_c);
        return YMODEM_ERR_ACK;
    }
    
    ctx->stage = YMODEM_STAGE_ESTABLISHED;
    ctx->packet_seq = 1; /* Start with packet 1 for actual data */
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Prepare file info packet (packet 0)
 */
static int _ymodem_prepare_file_info_packet(ymodem_context_t* ctx, const char* filename)
{
    uint8_t* data = ctx->buffer + 3; /* Skip header bytes (SOH/STX + seq + ~seq) */
    char file_size_str[20]; /* Big enough for uint64_t as string */
    size_t name_len;
    size_t size_len;
    
    /* Clear the packet data area */
    memset(data, 0, YMODEM_SOH_DATA_SIZE);
    
    /* Add filename */
    name_len = strlen(filename);
    if (name_len >= YMODEM_SOH_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    memcpy(data, filename, name_len);
    data[name_len] = '\0';
    
    /* Add file size */
    snprintf(file_size_str, sizeof(file_size_str), "%d", ctx->file_size);
    size_len = strlen(file_size_str);
    
    if (name_len + 1 + size_len >= YMODEM_SOH_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    memcpy(data + name_len + 1, file_size_str, size_len);
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Send a packet (header, sequence numbers, data and CRC)
 * 
 * @param ctx YMODEM context
 * @param code SOH or STX depending on packet size
 * @param seq Sequence number
 * @param data Data to send
 * @param data_size Size of data (128 or 1024)
 * @return int YMODEM_ERR_NONE on success or error code
 */
static int _ymodem_send_packet(ymodem_context_t* ctx, enum ymodem_code code, uint8_t seq, const uint8_t* data, size_t data_size) {
    uint16_t crc;
    size_t packet_size;
    
    /* Validate parameters */
    if (code == YMODEM_CODE_SOH && data_size != YMODEM_SOH_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    if (code == YMODEM_CODE_STX && data_size != YMODEM_STX_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    /* Calculate full packet size */
    packet_size = 1 + 1 + 1 + data_size + 2; /* header + seq + ~seq + data + CRC16 */
    
    /* Ensure our send buffer is large enough */
    if (ctx->send_buffer_size < packet_size) {
        return YMODEM_ERR_DSZ;
    }
    
    /* Construct the packet */
    ctx->send_buffer[0] = code;           /* SOH or STX */
    ctx->send_buffer[1] = seq;            /* Sequence number */
    ctx->send_buffer[2] = ~seq;           /* Complement of sequence number */
    
    /* Copy data */
    memcpy(&ctx->send_buffer[3], data, data_size);
    
    /* Calculate and append CRC */
    crc = ymodem_calc_crc16(&ctx->send_buffer[3], data_size);
    ctx->send_buffer[3 + data_size] = (crc >> 8) & 0xFF;    /* High byte of CRC */
    ctx->send_buffer[3 + data_size + 1] = crc & 0xFF;       /* Low byte of CRC */
    
    /* Send the packet */
    if (!ymodem_send_bytes(ctx, ctx->send_buffer, packet_size)) {
        return YMODEM_ERR_CODE;
    }
    
    return YMODEM_ERR_NONE;
}



/**
 * @brief Main data transfer loop
 */
// static int _ymodem_do_send_trans(ymodem_context_t* ctx) {
//     int ret;
//     size_t read_size;
//     enum ymodem_code packet_type;
//     int retries;
    
//     ctx->stage = YMODEM_STAGE_TRANSMITTING;
//     ctx->error_count = 0;
    
//     while (1) {
//         packet_type = YMODEM_CODE_STX;
//         read_size = YMODEM_STX_DATA_SIZE;
        
//         size_t actual_read = 0;
//         int retry_read;
        
//         for (retry_read = 0; retry_read < 10; retry_read++) {
//             actual_read += ctx->callbacks.file_read(ctx->file_handle, 
//                                                   ctx->buffer + 3 + actual_read, 
//                                                   read_size - actual_read);
//             if (actual_read == read_size)
//                 break;
//         }
//         YMODEM_DEBUG_PRINT("Read %zu bytes from file\n", actual_read);
        
//         if (actual_read == 0) {
//             break;
//         }
        
//         if (actual_read < read_size) {
//             memset(ctx->buffer + 3 + actual_read, 0x1A, read_size - actual_read);
//             ctx->stage = YMODEM_STAGE_FINISHING;
//         }
        
//         if (actual_read <= 128) {
//             packet_type = YMODEM_CODE_SOH;
//             read_size = YMODEM_SOH_DATA_SIZE;
//         } else {
//             packet_type = YMODEM_CODE_STX;
//         }
        
//         retries = 0;
//         while (retries < YMODEM_MAX_ERRORS) {
//             ret = _ymodem_send_packet(ctx, packet_type, ctx->packet_seq, ctx->buffer + 3, read_size);
//             if (ret != YMODEM_ERR_NONE) {
//                 retries++;
//                 continue;
//             }
            
//             ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//             if (ret == YMODEM_CODE_ACK) {
//                 YMODEM_DEBUG_PRINT("Packet #%d ACKed\n", ctx->packet_seq);
//                 break;
//             } else if (ret == YMODEM_CODE_CAN) {
//                 return YMODEM_ERR_CAN;
//             } else {
//                 retries++;
//             }
//             YMODEM_DEBUG_PRINT("Retry #%d for packet #%d\n", retries, ctx->packet_seq);
//         }
        
//         if (retries >= YMODEM_MAX_ERRORS) {
//             return YMODEM_ERR_ACK;
//         }
        
//         ctx->packet_seq = (ctx->packet_seq + 1) & 0xFF;
        
//         if (ctx->stage == YMODEM_STAGE_FINISHING) {
//             break;
//         }
//     }
    
//     return YMODEM_ERR_NONE;
// }
static int _ymodem_do_send_trans(ymodem_context_t* ctx) {
    int ret;
    size_t read_size;
    enum ymodem_code packet_type;
    int retries;
    
    ctx->stage = YMODEM_STAGE_TRANSMITTING;
    ctx->error_count = 0;
    
    while (1) {
        packet_type = YMODEM_CODE_STX;
        read_size = YMODEM_STX_DATA_SIZE;
        
        size_t actual_read = 0;
        int retry_read;
        
        for (retry_read = 0; retry_read < 10; retry_read++) {
            actual_read += ctx->callbacks.file_read(ctx->file_handle, 
                                                  ctx->buffer + 3 + actual_read, 
                                                  read_size - actual_read);
            if (actual_read == read_size)
                break;
        }
        YMODEM_DEBUG_PRINT("Read %zu bytes from file\n", actual_read);
        
        if (actual_read == 0) {
            break;
        }
        
        if (actual_read < read_size) {
            memset(ctx->buffer + 3 + actual_read, 0x1A, read_size - actual_read);
            ctx->stage = YMODEM_STAGE_FINISHING;
        }
        
        if (actual_read <= 128) {
            packet_type = YMODEM_CODE_SOH;
            read_size = YMODEM_SOH_DATA_SIZE;
        } else {
            packet_type = YMODEM_CODE_STX;
        }
        
        retries = 0;
        while (retries < YMODEM_MAX_ERRORS) {
            ret = _ymodem_send_packet(ctx, packet_type, ctx->packet_seq, ctx->buffer + 3, read_size);
            if (ret != YMODEM_ERR_NONE) {
                retries++;
                continue;
            }
            
            // 修改这里，使其更宽容地接受响应
            ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
            if (ret == YMODEM_CODE_ACK) {
                YMODEM_DEBUG_PRINT("Packet #%d ACKed\n", ctx->packet_seq);
                break;
            } else if (ret == YMODEM_CODE_NAK) {
                YMODEM_DEBUG_PRINT("Packet #%d NAKed, retrying\n", ctx->packet_seq);
                retries++;
            } else if (ret == YMODEM_CODE_C) {
                // 收到C也视为ACK，尤其是对于第一个数据包
                YMODEM_DEBUG_PRINT("Received 'C' instead of ACK for packet #%d, treating as ACK\n", ctx->packet_seq);
                break;
            } else if (ret == YMODEM_CODE_CAN) {
                return YMODEM_ERR_CAN;
            } else {
                YMODEM_DEBUG_PRINT("Unexpected response: %d\n", ret);
                retries++;
            }
            YMODEM_DEBUG_PRINT("Retry #%d for packet #%d\n", retries, ctx->packet_seq);
        }
        
        if (retries >= YMODEM_MAX_ERRORS) {
            return YMODEM_ERR_ACK;
        }
        
        ctx->packet_seq = (ctx->packet_seq + 1) & 0xFF;
        YMODEM_DEBUG_PRINT("Advancing to packet #%d\n", ctx->packet_seq);
        
        if (ctx->stage == YMODEM_STAGE_FINISHING) {
            break;
        }
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Finish the YMODEM transmission
 */
// static int _ymodem_do_send_fin(ymodem_context_t* ctx)
// {
//     int ret;
//     int retries;
    
//     ctx->stage = YMODEM_STAGE_FINISHING;
    
//     /* Send EOT and wait for NAK */
//     retries = 0;
//     while (retries < YMODEM_MAX_ERRORS) {
//         if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
//             return YMODEM_ERR_CODE;
//         }
//         YMODEM_DEBUG_PRINT("Sent first EOT, waiting for NAK...\n");
        
//         ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//         if (ret == YMODEM_CODE_NAK) {
//             break;
//         }
        
//         retries++;
//     }
    
//     if (retries >= YMODEM_MAX_ERRORS) {
//         return YMODEM_ERR_ACK;
//     }
    
//     /* Send second EOT and wait for ACK */
//     if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
//         return YMODEM_ERR_CODE;
//     }
//     YMODEM_DEBUG_PRINT("Sent second EOT, waiting for ACK...\n");
//     ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//     if (ret != YMODEM_CODE_ACK) {
//         return YMODEM_ERR_ACK;
//     }
    
//     /* Wait for C */
//     ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//     if (ret != YMODEM_CODE_C) {
//         return YMODEM_ERR_ACK;
//     }
//     YMODEM_DEBUG_PRINT("Sending NULL filename packet to indicate end of batch\n");
//     /* Prepare and send NULL filename packet to indicate end of batch */
//     memset(ctx->buffer + 3, 0, YMODEM_SOH_DATA_SIZE);
//     ret = _ymodem_send_packet(ctx, YMODEM_CODE_SOH, 0, ctx->buffer + 3, YMODEM_SOH_DATA_SIZE);
//     if (ret != YMODEM_ERR_NONE) {
//         return ret;
//     }
    
//     /* Wait for final ACK */
//     ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
//     if (ret != YMODEM_CODE_ACK) {
//         return YMODEM_ERR_ACK;
//     }

//     ctx->stage = YMODEM_STAGE_FINISHED;
//     return YMODEM_ERR_NONE;
// }
static int _ymodem_do_send_fin(ymodem_context_t* ctx)
{
    int ret;
    int retries;
    
    ctx->stage = YMODEM_STAGE_FINISHING;
    
    /* 发送EOT并等待NAK */
    retries = 0;
    while (retries < YMODEM_MAX_ERRORS) {
        if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
            return YMODEM_ERR_CODE;
        }
        YMODEM_DEBUG_PRINT("Sent first EOT, waiting for NAK...\n");
        
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        if (ret == YMODEM_CODE_NAK) {
            break;
        }
        
        retries++;
    }
    
    if (retries >= YMODEM_MAX_ERRORS) {
        return YMODEM_ERR_ACK;
    }
    
    /* 发送第二个EOT并等待ACK */
    retries = 0;
    while (retries < YMODEM_MAX_ERRORS) {
        if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
            return YMODEM_ERR_CODE;
        }
        YMODEM_DEBUG_PRINT("Sent second EOT, waiting for ACK...\n");
        
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        if (ret == YMODEM_CODE_ACK) {
            YMODEM_DEBUG_PRINT("Received ACK for second EOT\n");
            break;
        } else if (ret == YMODEM_CODE_NAK) {
            // 即使收到NAK也继续执行
            YMODEM_DEBUG_PRINT("Received NAK instead of ACK, continuing anyway...\n");
            break;
        }
        
        retries++;
    }
    
    if (retries >= YMODEM_MAX_ERRORS) {
        return YMODEM_ERR_ACK;
    }
    
    /* 等待C (可能已经接收到) */
    retries = 0;
    bool got_c = false;
    while (retries < YMODEM_MAX_ERRORS) {
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        if (ret == YMODEM_CODE_C) {
            YMODEM_DEBUG_PRINT("Received 'C' for NULL packet\n");
            got_c = true;
            break;
        } else if (ret == YMODEM_CODE_ACK) {
            // 可能同时接收到ACK和C，需要再次尝试接收C
            YMODEM_DEBUG_PRINT("Received ACK, waiting for 'C'...\n");
        } else {
            retries++;
        }
    }
    
    // 如果没有收到C，假装收到了继续执行
    if (!got_c) {
        YMODEM_DEBUG_PRINT("Did not receive 'C', continuing anyway...\n");
    }
    
    YMODEM_DEBUG_PRINT("Sending NULL filename packet to indicate end of batch\n");
    /* 准备并发送NULL文件名包，表示批处理结束 */
    memset(ctx->buffer + 3, 0, YMODEM_SOH_DATA_SIZE);
    ret = _ymodem_send_packet(ctx, YMODEM_CODE_SOH, 0, ctx->buffer + 3, YMODEM_SOH_DATA_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* 等待最后的ACK，但允许超时 */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_ACK) {
        YMODEM_DEBUG_PRINT("Did not receive final ACK, transmission still considered complete\n");
    } else {
        YMODEM_DEBUG_PRINT("Received final ACK, transmission complete\n");
    }

    ctx->stage = YMODEM_STAGE_FINISHED;
    return YMODEM_ERR_NONE;
}