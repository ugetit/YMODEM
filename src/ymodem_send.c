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
                    size_t buffer_size)
{
    if (ctx == NULL || callbacks == NULL || buffer == NULL) {
        return YMODEM_ERR_CODE;
    }
    
    if (buffer_size < YMODEM_MAX_PACKET_SIZE) {
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
    
    /* Send file data */
    ret = _ymodem_do_send_trans(ctx);
    if (ret != YMODEM_ERR_NONE) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
        return ret;
    }
    
    /* Finish transmission */
    ret = _ymodem_do_send_fin(ctx);
    
    /* Close file */
    ctx->callbacks.file_close(ctx->file_handle);
    ctx->file_handle = NULL;
    
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
static int _ymodem_do_send_handshake(ymodem_context_t* ctx, int timeout_s)
{
    int i;
    int ret;
    
    ctx->stage = YMODEM_STAGE_ESTABLISHING;
    
    /* Wait for 'C' character to start transfer */
    for (i = 0; i < timeout_s; i++) {
        ret = ymodem_receive_byte(ctx, YMODEM_HANDSHAKE_INTERVAL_MS);
        if (ret == YMODEM_CODE_C) {
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
    
    /* Wait for ACK */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_ACK) {
        return YMODEM_ERR_ACK;
    }
    
    /* Wait for 'C' */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_C) {
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
static int _ymodem_send_packet(ymodem_context_t* ctx, enum ymodem_code code, uint8_t seq, const uint8_t* data, size_t data_size)
{
    size_t i;
    uint16_t crc;
    
    /* Check packet size */
    if (code == YMODEM_CODE_SOH && data_size != YMODEM_SOH_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    if (code == YMODEM_CODE_STX && data_size != YMODEM_STX_DATA_SIZE) {
        return YMODEM_ERR_DSZ;
    }
    
    /* Send packet header byte (SOH or STX) */
    if (!ymodem_send_byte(ctx, code)) {
        return YMODEM_ERR_CODE;
    }
    
    /* Send sequence number and complement */
    if (!ymodem_send_byte(ctx, seq) || !ymodem_send_byte(ctx, ~seq)) {
        return YMODEM_ERR_CODE;
    }
    
    /* Send data */
    for (i = 0; i < data_size; i++) {
        if (!ymodem_send_byte(ctx, data[i])) {
            return YMODEM_ERR_CODE;
        }
    }
    
    /* Calculate and send CRC */
    crc = ymodem_calc_crc16(data, data_size);
    if (!ymodem_send_byte(ctx, (crc >> 8) & 0xFF) || !ymodem_send_byte(ctx, crc & 0xFF)) {
        return YMODEM_ERR_CODE;
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Main data transfer loop
 */
static int _ymodem_do_send_trans(ymodem_context_t* ctx)
{
    int ret;
    size_t read_size;
    enum ymodem_code packet_type;
    int retries;
    
    ctx->stage = YMODEM_STAGE_TRANSMITTING;
    ctx->error_count = 0;
    
    while (1) {
        /* Read data from file */
        if (ctx->file_size > 0 && ctx->callbacks.file_size != NULL) {
            int remaining = ctx->file_size - ctx->callbacks.file_size(ctx->file_handle);
            if (remaining <= 0) {
                /* End of file reached */
                break;
            }
            
            /* Decide packet size based on remaining data */
            if (remaining > YMODEM_STX_DATA_SIZE) {
                packet_type = YMODEM_CODE_STX;
                read_size = YMODEM_STX_DATA_SIZE;
            } else {
                packet_type = YMODEM_CODE_SOH;
                read_size = YMODEM_SOH_DATA_SIZE;
            }
        } else {
            /* File size unknown, try to read maximum packet size */
            packet_type = YMODEM_CODE_STX;
            read_size = YMODEM_STX_DATA_SIZE;
        }
        
        /* Read data from file to buffer (after header) */
        memset(ctx->buffer + 3, 0x1A, read_size); /* Pad with SUB (0x1A) */
        
        size_t actual_read = ctx->callbacks.file_read(ctx->file_handle, ctx->buffer + 3, read_size);
        if (actual_read == 0) {
            /* End of file reached */
            break;
        }
        
        /* If we read less than a full packet, use SOH instead of STX */
        if (actual_read < YMODEM_STX_DATA_SIZE && packet_type == YMODEM_CODE_STX) {
            packet_type = YMODEM_CODE_SOH;
            read_size = YMODEM_SOH_DATA_SIZE;
        }
        
        /* Send the packet with retries */
        retries = 0;
        while (retries < YMODEM_MAX_ERRORS) {
            /* Send data packet */
            ret = _ymodem_send_packet(ctx, packet_type, ctx->packet_seq, ctx->buffer + 3, read_size);
            if (ret != YMODEM_ERR_NONE) {
                retries++;
                continue;
            }
            
            /* Wait for ACK */
            ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
            if (ret == YMODEM_CODE_ACK) {
                /* Packet successfully transmitted */
                break;
            } else if (ret == YMODEM_CODE_CAN) {
                /* Transmission cancelled by receiver */
                return YMODEM_ERR_CAN;
            } else {
                /* NAK or timeout, retry */
                retries++;
            }
        }
        
        if (retries >= YMODEM_MAX_ERRORS) {
            return YMODEM_ERR_ACK;
        }
        
        /* Increment sequence number for next packet */
        ctx->packet_seq = (ctx->packet_seq + 1) & 0xFF;
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Finish the YMODEM transmission
 */
static int _ymodem_do_send_fin(ymodem_context_t* ctx)
{
    int ret;
    int retries;
    
    ctx->stage = YMODEM_STAGE_FINISHING;
    
    /* Send EOT and wait for NAK */
    retries = 0;
    while (retries < YMODEM_MAX_ERRORS) {
        if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
            return YMODEM_ERR_CODE;
        }
        
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        if (ret == YMODEM_CODE_NAK) {
            break;
        }
        
        retries++;
    }
    
    if (retries >= YMODEM_MAX_ERRORS) {
        return YMODEM_ERR_ACK;
    }
    
    /* Send second EOT and wait for ACK */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_EOT)) {
        return YMODEM_ERR_CODE;
    }
    
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_ACK) {
        return YMODEM_ERR_ACK;
    }
    
    /* Wait for C */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_C) {
        return YMODEM_ERR_ACK;
    }
    
    /* Prepare and send NULL filename packet to indicate end of batch */
    memset(ctx->buffer + 3, 0, YMODEM_SOH_DATA_SIZE);
    ret = _ymodem_send_packet(ctx, YMODEM_CODE_SOH, 0, ctx->buffer + 3, YMODEM_SOH_DATA_SIZE);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* Wait for final ACK */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret != YMODEM_CODE_ACK) {
        return YMODEM_ERR_ACK;
    }
    
    ctx->stage = YMODEM_STAGE_FINISHED;
    return YMODEM_ERR_NONE;
}