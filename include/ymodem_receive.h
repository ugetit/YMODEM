/**
 * @file ymodem_receive.h
 * @brief YMODEM receiver implementation header
 * @date 2025-04-09
 * 
 * This file contains the API for the YMODEM receiver implementation.
 */

#ifndef __YMODEM_RECEIVE_H__
#define __YMODEM_RECEIVE_H__

#include "ymodem_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize YMODEM context for receiving
 * 
 * @param ctx Pointer to YMODEM context
 * @param callbacks Callback functions
 * @param buffer Buffer for YMODEM data (must be at least YMODEM_MAX_PACKET_SIZE bytes)
 * @param buffer_size Size of the provided buffer
 * @return int YMODEM_ERR_NONE on success, error code otherwise
 */
int ymodem_receive_init(ymodem_context_t* ctx, 
                       const ymodem_callbacks_t* callbacks,
                       uint8_t* buffer,
                       size_t buffer_size);

/**
 * @brief Receive a file via YMODEM protocol
 * 
 * This function handles the complete YMODEM receive process including handshake,
 * receiving data packets, and finishing the transmission.
 * 
 * @param ctx Pointer to initialized YMODEM context
 * @param file_info Pointer to struct that will be filled with received file info
 * @param handshake_timeout_s Timeout for handshake in seconds
 * @return int YMODEM_ERR_NONE on success, error code otherwise
 */
int ymodem_receive_file(ymodem_context_t* ctx, 
                       ymodem_file_info_t* file_info,
                       int handshake_timeout_s);

/**
 * @brief Clean up YMODEM receive resources
 * 
 * @param ctx Pointer to YMODEM context
 */
void ymodem_receive_cleanup(ymodem_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* __YMODEM_RECEIVE_H__ */