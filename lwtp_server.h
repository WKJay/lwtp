#ifndef LWTP_SERVER_H
#define LWTP_SERVER_H

#include <stdint.h>
#include "lwtp_user_cfg.h"

typedef enum { DS_HEAD = 0, DS_LENGTH, DS_DATA, DS_END, DS_FINISH } decoder_step_t;

enum {
    LWTP_ACK_OK = 0,
    LWTP_ACK_ERR = 1,
};

enum {
    LWTP_ERR_CODE_NULL = 0,
    LWTP_ERR_CODE_CMD = 1,
    LWTP_ERR_CODE_DATA_LEN = 2,
    LWTP_ERR_CODE_DATA_VALUE = 3,
    LWTP_ERR_CODE_EXEC = 4,

    LWTP_ERR_CODE_INTERNAL = 20,  // 20 以后的错误可由用户自定义
};

typedef struct _cmd_process_info {
    uint32_t cmd;
    uint8_t *rx_data;
    uint32_t rx_data_len;
    uint8_t *tx_data_buf;
    uint32_t tx_data_buf_sz;
    uint32_t tx_data_len;
    uint8_t ack_status;
    uint8_t ack_code;
} cmd_process_info_t;
typedef struct _cmd_handler {
    uint32_t cmd;                             // 功能码
    int (*handler)(cmd_process_info_t *cpi);  // 处理函数
    struct _cmd_handler *next;
} cmd_handler_t;
typedef struct _decoder {
    decoder_step_t step;
    uint16_t data_len;
    uint16_t len_to_read;
} decoder_t;
typedef struct _lwtp_server_session {
    uint8_t rx_buf[LWTP_RX_BUF_SZ];
    uint8_t tx_buf[LWTP_TX_BUF_SZ];
    decoder_t decoder;
    cmd_process_info_t cpi;
} lwtp_server_session_t;

typedef struct _lwtp_server {
    cmd_handler_t *cmd_handler;
} lwtp_server_t;

#define LWTP_CRC_LEN      2
#define LWTP_REQ_INFO_LEN (LWTP_REQ_LEN_OFFSET + LWTP_REQ_LEN_SZ + LWTP_CRC_LEN)

#define LWTP_RSP_DATA_OFFSET 6
#define LWTP_RSP_INFO_LEN    (LWTP_RSP_DATA_OFFSET + LWTP_CRC_LEN)

int lwtp_server_session_recv_process(lwtp_server_t *lwtp, lwtp_server_session_t *s, uint8_t *data, uint32_t data_len,
                                     void (*send_data)(uint8_t *data, uint32_t len));
int lwtp_server_cmd_handler_register(lwtp_server_t *lwtp, cmd_handler_t *ch);
int lwtp_server_session_init(lwtp_server_session_t *s);
int lwtp_server_init(lwtp_server_t *server);

#endif  // LWTP_SERVER_H
