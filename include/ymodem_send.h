/**
 * @file ymodem_send.h
 * @brief YMODEM sender implementation header
 * @date 2025-04-09
 * 
 * This file contains the API for the YMODEM sender implementation.
 */

#ifndef __YMODEM_SEND_H__
#define __YMODEM_SEND_H__

#include "ymodem_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize YMODEM context for sending
 * 
 * @param ctx Pointer to YMODEM context
 * @param callbacks Callback functions
 * @param buffer Buffer for YMODEM data (must be at least YMODEM_MAX_PACKET_SIZE bytes)
 * @param buffer_size Size of the provided buffer
 * @return int YMODEM_ERR_NONE on success, error code otherwise
 */
int ymodem_send_init(ymodem_context_t* ctx, 
                    const ymodem_callbacks_t* callbacks,
                    uint8_t* buffer,
                    size_t buffer_size);

/**
 * @brief Send a file via YMODEM protocol
 * 
 * This function handles the complete YMODEM send process including handshake,
 * sending data packets, and finishing the transmission.
 * 
 * @param ctx Pointer to initialized YMODEM context
 * @param filename Full path to the file to send
 * @param handshake_timeout_s Timeout for handshake in seconds
 * @return int YMODEM_ERR_NONE on success, error code otherwise
 */
int ymodem_send_file(ymodem_context_t* ctx, 
                    const char* filename,
                    int handshake_timeout_s);

/**
 * @brief Clean up YMODEM send resources
 * 
 * @param ctx Pointer to YMODEM context
 */
void ymodem_send_cleanup(ymodem_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* __YMODEM_SEND_H__ */