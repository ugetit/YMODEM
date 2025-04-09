/**
 * @file ymodem_receive.c
 * @brief YMODEM receiver implementation
 * @date 2025-04-09
 * 
 * This file contains the implementation of the YMODEM receiver.
 */

#include "ymodem_receive.h"

/* Forward declarations of internal functions */
static int _ymodem_do_handshake(ymodem_context_t* ctx, int timeout_s);
static int _ymodem_receive_packet(ymodem_context_t* ctx, uint8_t* seq, size_t* data_size);
static int _ymodem_process_packet(ymodem_context_t* ctx, uint8_t seq, const uint8_t* data, size_t data_size);
static int _ymodem_do_trans(ymodem_context_t* ctx);
static int _ymodem_do_fin(ymodem_context_t* ctx);
static int _ymodem_parse_file_info(ymodem_context_t* ctx, ymodem_file_info_t* file_info);

/**
 * @brief Initialize YMODEM context for receiving
 */
int ymodem_receive_init(ymodem_context_t* ctx, 
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
        callbacks->file_write == NULL || 
        callbacks->file_close == NULL) {
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
 * @brief Receive a file via YMODEM protocol
 */
int ymodem_receive_file(ymodem_context_t* ctx, 
                       ymodem_file_info_t* file_info,
                       int handshake_timeout_s)
{
    int ret;
    
    if (ctx == NULL || file_info == NULL) {
        return YMODEM_ERR_CODE;
    }
    
    /* Start handshake */
    ret = _ymodem_do_handshake(ctx, handshake_timeout_s);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* Parse file info from packet 0 */
    ret = _ymodem_parse_file_info(ctx, file_info);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* Open file for writing */
    ctx->file_handle = ctx->callbacks.file_open(file_info->filename, true);
    if (ctx->file_handle == NULL) {
        return YMODEM_ERR_FILE;
    }
    
    /* Receive file data */
    ret = _ymodem_do_trans(ctx);
    if (ret != YMODEM_ERR_NONE) {
        ctx->callbacks.file_close(ctx->file_handle);
        ctx->file_handle = NULL;
        return ret;
    }
    
    /* Finish transmission */
    ret = _ymodem_do_fin(ctx);
    
    /* Close file */
    ctx->callbacks.file_close(ctx->file_handle);
    ctx->file_handle = NULL;
    
    return ret;
}

/**
 * @brief Clean up YMODEM receive resources
 */
void ymodem_receive_cleanup(ymodem_context_t* ctx)
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
 * @brief Perform YMODEM handshake
 * 
 * Send 'C' characters to initiate CRC mode transfer until a valid response is received.
 */
static int _ymodem_do_handshake(ymodem_context_t* ctx, int timeout_s)
{
    int i;
    enum ymodem_code code;
    uint8_t seq;
    size_t data_size;
    int ret;
    
    ctx->stage = YMODEM_STAGE_ESTABLISHING;
    
    /* Send 'C' periodically until we get a response or timeout */
    for (i = 0; i < timeout_s; i++) {
        /* Send 'C' character to request CRC mode */
        if (!ymodem_send_byte(ctx, YMODEM_CODE_C)) {
            return YMODEM_ERR_CODE;
        }
        
        /* Wait for SOH or STX */
        ret = ymodem_receive_byte(ctx, YMODEM_HANDSHAKE_INTERVAL_MS);
        if (ret >= 0) {
            code = ret;
            if (code == YMODEM_CODE_SOH || code == YMODEM_CODE_STX) {
                ctx->buffer[0] = (uint8_t)code;
                break;
            }
        }
    }
    
    if (i == timeout_s) {
        return YMODEM_ERR_TMO;
    }
    
    /* Receive the rest of the packet */
    ret = _ymodem_receive_packet(ctx, &seq, &data_size);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* First packet (packet 0) must have sequence number 0 */
    if (seq != 0) {
        return YMODEM_ERR_SEQ;
    }
    
    /* We got packet 0, now we're established */
    ctx->stage = YMODEM_STAGE_ESTABLISHED;
    
    /* ACK the packet and send another 'C' to start data transfer */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_ACK) ||
        !ymodem_send_byte(ctx, YMODEM_CODE_C)) {
        return YMODEM_ERR_CODE;
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Parse file information from packet 0
 */
static int _ymodem_parse_file_info(ymodem_context_t* ctx, ymodem_file_info_t* file_info)
{
    char* filename;
    char* file_size_str;
    size_t name_len;
    
    /* Packet 0 contains filename and optionally file size */
    filename = (char*)(ctx->buffer + 3); /* Skip SOH/STX + seq + ~seq */
    
    /* Check if this is an empty packet (end of batch) */
    if (filename[0] == '\0') {
        return YMODEM_ERR_FILE;
    }
    
    /* Find the end of filename */
    name_len = 0;
    while (filename[name_len] != '\0' && name_len < YMODEM_MAX_FILENAME_LENGTH - 1) {
        name_len++;
    }
    
    if (name_len == 0) {
        return YMODEM_ERR_FILE;
    }
    
    /* Copy filename */
    if (name_len >= YMODEM_MAX_FILENAME_LENGTH) {
        name_len = YMODEM_MAX_FILENAME_LENGTH - 1;
    }
    
    /* Copy filename to context and file_info */
    memcpy(ctx->filename, filename, name_len);
    ctx->filename[name_len] = '\0';
    memcpy(file_info->filename, filename, name_len);
    file_info->filename[name_len] = '\0';
    
    /* Get file size if available */
    file_size_str = filename + name_len + 1;
    if (file_size_str[0] != '\0') {
        /* Convert file size string to integer */
        ctx->file_size = 0;
        while (*file_size_str >= '0' && *file_size_str <= '9') {
            ctx->file_size = ctx->file_size * 10 + (*file_size_str - '0');
            file_size_str++;
        }
        file_info->filesize = ctx->file_size;
    } else {
        /* File size not provided */
        ctx->file_size = 0;
        file_info->filesize = 0;
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Receive a packet (sequence numbers, data and CRC)
 */
static int _ymodem_receive_packet(ymodem_context_t* ctx, uint8_t* seq, size_t* data_size)
{
    size_t i;
    uint16_t received_crc, calculated_crc;
    size_t packet_size;
    uint8_t* buf = ctx->buffer;
    
    /* Determine packet size based on header byte */
    if (buf[0] == YMODEM_CODE_SOH) {
        packet_size = YMODEM_SOH_PACKET_SIZE;
        *data_size = YMODEM_SOH_DATA_SIZE;
    } else if (buf[0] == YMODEM_CODE_STX) {
        packet_size = YMODEM_STX_PACKET_SIZE;
        *data_size = YMODEM_STX_DATA_SIZE;
    } else {
        return YMODEM_ERR_CODE;
    }
    
    /* We already have the first byte, receive the rest */
    for (i = 1; i < packet_size; i++) {
        int ret = ymodem_receive_byte(ctx, YMODEM_WAIT_CHAR_TIMEOUT_MS);
        if (ret < 0) {
            return YMODEM_ERR_TMO;
        }
        buf[i] = (uint8_t)ret;
    }
    
    /* Check sequence numbers */
    *seq = buf[1];
    if (buf[2] != (uint8_t)(~(*seq))) {
        return YMODEM_ERR_SEQ;
    }
    
    /* Verify CRC */
    received_crc = (uint16_t)(buf[packet_size - 2] << 8) | buf[packet_size - 1];
    calculated_crc = ymodem_calc_crc16(buf + 3, *data_size);
    
    if (received_crc != calculated_crc) {
        return YMODEM_ERR_CRC;
    }
    
    return YMODEM_ERR_NONE;
}

/**
 * @brief Main data transfer loop
 */
static int _ymodem_do_trans(ymodem_context_t* ctx)
{
    int ret;
    enum ymodem_code code;
    uint8_t seq;
    size_t data_size;
    uint8_t expected_seq = 1; /* We expect packet 1 after packet 0 */
    
    ctx->stage = YMODEM_STAGE_TRANSMITTING;
    ctx->error_count = 0;
    
    while (1) {
        /* Wait for SOH/STX/EOT */
        ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
        if (ret < 0) {
            return YMODEM_ERR_TMO;
        }
        
        code = ret;
        ctx->buffer[0] = (uint8_t)code;
        
        /* Check for end of transmission */
        if (code == YMODEM_CODE_EOT) {
            return YMODEM_ERR_NONE;
        }
        
        /* Check for valid packet start */
        if (code != YMODEM_CODE_SOH && code != YMODEM_CODE_STX) {
            ctx->error_count++;
            if (ctx->error_count > YMODEM_MAX_ERRORS) {
                return YMODEM_ERR_CODE;
            }
            
            /* Request retransmission */
            if (!ymodem_send_byte(ctx, YMODEM_CODE_NAK)) {
                return YMODEM_ERR_CODE;
            }
            continue;
        }
        
        /* Receive the rest of the packet */
        ret = _ymodem_receive_packet(ctx, &seq, &data_size);
        if (ret != YMODEM_ERR_NONE) {
            ctx->error_count++;
            if (ctx->error_count > YMODEM_MAX_ERRORS) {
                return ret;
            }
            
            /* Request retransmission */
            if (!ymodem_send_byte(ctx, YMODEM_CODE_NAK)) {
                return YMODEM_ERR_CODE;
            }
            continue;
        }
        
        /* Reset error counter on successful packet */
        ctx->error_count = 0;
        
        /* Check sequence number */
        if (seq != expected_seq) {
            /* Out of sequence packet - possible duplicate or missed packet */
            if (!ymodem_send_byte(ctx, YMODEM_CODE_NAK)) {
                return YMODEM_ERR_CODE;
            }
            continue;
        }
        
        /* Process packet data */
        if (ctx->file_handle != NULL) {
            /* Write data to file */
            size_t written = ctx->callbacks.file_write(
                ctx->file_handle, 
                ctx->buffer + 3, /* Skip SOH/STX + seq + ~seq */
                data_size
            );
            
            if (written != data_size) {
                return YMODEM_ERR_FILE;
            }
        }
        
        /* ACK the packet */
        if (!ymodem_send_byte(ctx, YMODEM_CODE_ACK)) {
            return YMODEM_ERR_CODE;
        }
        
        /* Increment sequence number for next packet */
        expected_seq = (expected_seq + 1) & 0xFF;
    }
}

/**
 * @brief Finish the YMODEM transmission
 */
static int _ymodem_do_fin(ymodem_context_t* ctx)
{
    int ret;
    uint8_t seq;
    size_t data_size;
    
    ctx->stage = YMODEM_STAGE_FINISHING;
    
    /* We've already received one EOT, send NAK to request final confirmation */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_NAK)) {
        return YMODEM_ERR_CODE;
    }
    
    /* Wait for second EOT */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret < 0 || ret != YMODEM_CODE_EOT) {
        return YMODEM_ERR_CODE;
    }
    
    /* Send ACK for the EOT */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_ACK)) {
        return YMODEM_ERR_CODE;
    }
    
    /* Send C to request final NULL packet */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_C)) {
        return YMODEM_ERR_CODE;
    }
    
    /* Wait for final packet header */
    ret = ymodem_receive_byte(ctx, YMODEM_WAIT_PACKET_TIMEOUT_MS);
    if (ret < 0) {
        return YMODEM_ERR_TMO;
    }
    
    ctx->buffer[0] = (uint8_t)ret;
    
    /* Check header */
    if (ctx->buffer[0] != YMODEM_CODE_SOH && ctx->buffer[0] != YMODEM_CODE_STX) {
        return YMODEM_ERR_CODE;
    }
    
    /* Receive the rest of the packet */
    ret = _ymodem_receive_packet(ctx, &seq, &data_size);
    if (ret != YMODEM_ERR_NONE) {
        return ret;
    }
    
    /* Final packet should have sequence number 0 */
    if (seq != 0) {
        return YMODEM_ERR_SEQ;
    }
    
    /* Check if it's a NULL filename packet (end of batch) */
    if (ctx->buffer[3] == 0) {
        ctx->stage = YMODEM_STAGE_FINISHED;
        
        /* Send final ACK */
        if (!ymodem_send_byte(ctx, YMODEM_CODE_ACK)) {
            return YMODEM_ERR_CODE;
        }
        
        return YMODEM_ERR_NONE;
    }
    
    /* If not a NULL filename packet, it's start of next file */
    /* We don't handle multiple files in one session, so just ACK and return */
    if (!ymodem_send_byte(ctx, YMODEM_CODE_ACK)) {
        return YMODEM_ERR_CODE;
    }
    
    return YMODEM_ERR_NONE;
}