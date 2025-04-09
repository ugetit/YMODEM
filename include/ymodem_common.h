/**
 * @file ymodem_common.h
 * @brief Common definitions for portable YMODEM implementation
 * @date 2025-04-09
 * 
 * This file contains common definitions, enums, and callback types
 * for a portable YMODEM implementation.
 */

#ifndef __YMODEM_COMMON_H__
#define __YMODEM_COMMON_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* YMODEM protocol codes */
enum ymodem_code {
    YMODEM_CODE_NONE = 0x00,
    YMODEM_CODE_SOH  = 0x01,  /* Start of header (128 byte data) */
    YMODEM_CODE_STX  = 0x02,  /* Start of header (1024 byte data) */
    YMODEM_CODE_EOT  = 0x04,  /* End of transmission */
    YMODEM_CODE_ACK  = 0x06,  /* Acknowledge */
    YMODEM_CODE_NAK  = 0x15,  /* Negative acknowledge */
    YMODEM_CODE_CAN  = 0x18,  /* Cancel transmission */
    YMODEM_CODE_C    = 0x43,  /* ASCII 'C' - CRC mode */
};

/* YMODEM error codes */
enum ymodem_error {
    YMODEM_ERR_NONE  = 0,     /* No error */
    YMODEM_ERR_TMO   = -1,    /* Timeout on handshake */
    YMODEM_ERR_CODE  = -2,    /* Wrong code, wrong SOH, STX etc */
    YMODEM_ERR_SEQ   = -3,    /* Wrong sequence number */
    YMODEM_ERR_CRC   = -4,    /* Wrong CRC checksum */
    YMODEM_ERR_DSZ   = -5,    /* Not enough data received */
    YMODEM_ERR_CAN   = -6,    /* The transmission is aborted by user */
    YMODEM_ERR_ACK   = -7,    /* Wrong answer, wrong ACK or C */
    YMODEM_ERR_FILE  = -8,    /* File operation error */
    YMODEM_ERR_MEM   = -9,    /* Memory allocation error */
};

/* YMODEM stages */
enum ymodem_stage {
    YMODEM_STAGE_NONE = 0,
    YMODEM_STAGE_ESTABLISHING,    /* Set when C is sent */
    YMODEM_STAGE_ESTABLISHED,     /* Set when we've got packet 0 and sent ACK and second C */
    YMODEM_STAGE_TRANSMITTING,    /* Set when receiving/sending actual data packets */
    YMODEM_STAGE_FINISHING,       /* Set when EOT is sent/received */
    YMODEM_STAGE_FINISHED,        /* Set when transmission is really finished */
};

/* Default YMODEM settings */
#ifndef YMODEM_WAIT_CHAR_TIMEOUT_MS
#define YMODEM_WAIT_CHAR_TIMEOUT_MS     3000  /* 3 seconds timeout for character */
#endif

#ifndef YMODEM_WAIT_PACKET_TIMEOUT_MS
#define YMODEM_WAIT_PACKET_TIMEOUT_MS   3000  /* 3 seconds timeout for packet */
#endif

#ifndef YMODEM_HANDSHAKE_INTERVAL_MS
#define YMODEM_HANDSHAKE_INTERVAL_MS    1000  /* 1 second between handshake attempts */
#endif

#ifndef YMODEM_MAX_ERRORS
#define YMODEM_MAX_ERRORS               5     /* Maximum number of errors before aborting */
#endif

#ifndef YMODEM_CAN_SEND_COUNT
#define YMODEM_CAN_SEND_COUNT           7     /* Number of CAN bytes to send when cancelling */
#endif

/* YMODEM packet sizes */
#define YMODEM_SOH_DATA_SIZE            128   /* SOH data size */
#define YMODEM_STX_DATA_SIZE            1024  /* STX data size */

#define YMODEM_SOH_PACKET_SIZE          (1+2+YMODEM_SOH_DATA_SIZE+2)   /* SOH + seq + ~seq + data + CRC16 */
#define YMODEM_STX_PACKET_SIZE          (1+2+YMODEM_STX_DATA_SIZE+2)   /* STX + seq + ~seq + data + CRC16 */

#define YMODEM_MAX_PACKET_SIZE          YMODEM_STX_PACKET_SIZE         /* Maximum packet size */
#define YMODEM_MAX_FILENAME_LENGTH      256   /* Maximum filename length */

/* YMODEM file info structure */
typedef struct {
    char    filename[YMODEM_MAX_FILENAME_LENGTH];  /* File name */
    size_t  filesize;                              /* File size */
} ymodem_file_info_t;

/* File operation callbacks */
typedef void* (*ymodem_file_open_func)(const char* filename, bool writing);
typedef size_t (*ymodem_file_read_func)(void* file_handle, uint8_t* buffer, size_t size);
typedef size_t (*ymodem_file_write_func)(void* file_handle, const uint8_t* buffer, size_t size);
typedef void (*ymodem_file_close_func)(void* file_handle);
typedef int (*ymodem_file_size_func)(void* file_handle);

/* Communication callbacks */
typedef bool (*ymodem_comm_send_func)(uint8_t data);
typedef int (*ymodem_comm_receive_func)(uint8_t* data, uint32_t timeout_ms);

/* Timing callbacks */
typedef uint32_t (*ymodem_get_time_ms_func)(void);
typedef void (*ymodem_delay_ms_func)(uint32_t ms);

/* Callback collection structure */
typedef struct {
    /* File operation callbacks */
    ymodem_file_open_func     file_open;
    ymodem_file_read_func     file_read;
    ymodem_file_write_func    file_write;
    ymodem_file_close_func    file_close;
    ymodem_file_size_func     file_size;
    
    /* Communication callbacks */
    ymodem_comm_send_func     comm_send;
    ymodem_comm_receive_func  comm_receive;
    
    /* Timing callbacks */
    ymodem_get_time_ms_func   get_time_ms;
    ymodem_delay_ms_func      delay_ms;
} ymodem_callbacks_t;

/* YMODEM context structure */
typedef struct {
    ymodem_callbacks_t callbacks;        /* Registered callbacks */
    enum ymodem_stage  stage;            /* Current transmission stage */
    uint8_t*           buffer;           /* Data buffer */
    size_t             buffer_size;      /* Buffer size */
    void*              file_handle;      /* Current file handle */
    int                file_size;        /* Current file size */
    char               filename[YMODEM_MAX_FILENAME_LENGTH]; /* Current filename */
    uint8_t            packet_seq;       /* Current packet sequence number */
    uint8_t            error_count;      /* Error counter */
} ymodem_context_t;

/* Common helper functions */
uint16_t ymodem_calc_crc16(const uint8_t* buffer, size_t size);
const char* ymodem_get_path_basename(const char* path);
bool ymodem_send_byte(ymodem_context_t* ctx, uint8_t data);
int ymodem_receive_byte(ymodem_context_t* ctx, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __YMODEM_COMMON_H__ */