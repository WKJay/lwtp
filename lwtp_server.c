/***************************************************************
 * @file           lwtp_server.c
 * @brief          Lightweight Transfer Protocol Server
 * @author         WKJay
 * @Version
 * @Date           2024-06-12
 ***************************************************************/
#include <string.h>
#include "lwtp_utils.h"
#include "lwtp_server.h"

static int lwtp_server_cmd_process(lwtp_server_t *lwtp, lwtp_server_session_t *s) {
    int ret = -1;
    cmd_handler_t *ch = lwtp->cmd_handler;
    cmd_process_info_t *cpi = &s->cpi;
    cpi->ack_status = LWTP_ACK_ERR;
    cpi->ack_code = LWTP_ERR_CODE_CMD;
    cpi->tx_data_len = 0;

    while (ch) {
        if (ch->cmd == cpi->cmd) {
            cpi->ack_status = LWTP_ACK_OK;
            cpi->ack_code = LWTP_ERR_CODE_NULL;
            // LWTP_LOG("cmd %d comes in\n", cpi->cmd);
            ret = ch->handler(cpi);
            goto next;
        }
        ch = ch->next;
    }
    LWTP_LOG("cmd %d not found\n", cpi->cmd);

next:
    if (ret < 0) {
        if (cpi->ack_status == LWTP_ACK_OK) {
            cpi->ack_status = LWTP_ACK_ERR;
            cpi->ack_code = LWTP_ERR_CODE_INTERNAL;
        }
        cpi->tx_data_len = 0;
    }

    // build frame info
    s->tx_buf[0] = LWTP_HEAD;
    s->tx_buf[1] = cpi->cmd;
    s->tx_buf[2] = cpi->ack_status;
    s->tx_buf[3] = cpi->ack_code;
    s->tx_buf[4] = cpi->tx_data_len >> 8;
    s->tx_buf[5] = cpi->tx_data_len & 0xFF;

    // build crc
    uint16_t crc = lwtp_crc16(s->tx_buf, LWTP_RSP_DATA_OFFSET + cpi->tx_data_len);
    s->tx_buf[LWTP_RSP_DATA_OFFSET + cpi->tx_data_len] = crc >> 8;
    s->tx_buf[LWTP_RSP_DATA_OFFSET + cpi->tx_data_len + 1] = crc & 0xFF;

    return LWTP_RSP_INFO_LEN + cpi->tx_data_len;
}

static int lwtp_server_session_recv_preprocess(lwtp_server_session_t *s, uint8_t *data, uint16_t data_len) {
    int ret = -1;
    uint16_t recv_crc, calc_crc;
    if (data_len == 0) return 0;

    switch (s->decoder.step) {
        case DS_HEAD:
            if (data[0] == LWTP_HEAD) {
                s->rx_buf[0] = data[0];
                ret = 1;
                s->decoder.data_len = 1;
                // 此处需要读取从数据头结束到数据长度结束的数据
                s->decoder.len_to_read = (LWTP_REQ_LEN_OFFSET - LWTP_REQ_HEAD_SZ) + LWTP_REQ_LEN_SZ;
                s->decoder.step = DS_LENGTH;
            } else {
                ret = -1;
            }
            break;
        case DS_LENGTH:
            if (data_len < s->decoder.len_to_read) {
                memcpy(s->rx_buf + s->decoder.data_len, data, data_len);
                ret = data_len;
                s->decoder.data_len += data_len;
                s->decoder.len_to_read -= data_len;
            } else {
                memcpy(s->rx_buf + s->decoder.data_len, data, s->decoder.len_to_read);
                ret = s->decoder.len_to_read;
                s->decoder.data_len += s->decoder.len_to_read;
                s->decoder.len_to_read = lwtp_byte2_to_uint16(s->rx_buf + LWTP_REQ_LEN_OFFSET);
                if (s->decoder.len_to_read + LWTP_REQ_INFO_LEN > LWTP_RX_BUF_SZ) {
                    LWTP_LOG("data length %d is too long\n", s->decoder.len_to_read);
                    ret = -1;
                    break;
                }
                s->decoder.step = DS_DATA;
            }
            break;
        case DS_DATA:
            if (data_len < s->decoder.len_to_read) {
                memcpy(s->rx_buf + s->decoder.data_len, data, data_len);
                ret = data_len;
                s->decoder.data_len += data_len;
                s->decoder.len_to_read -= data_len;
            } else {
                memcpy(s->rx_buf + s->decoder.data_len, data, s->decoder.len_to_read);
                ret = s->decoder.len_to_read;
                s->decoder.data_len += s->decoder.len_to_read;
                s->decoder.len_to_read = LWTP_CRC_LEN;
                s->decoder.step = DS_END;
            }
            break;
        case DS_END:
            if (data_len < s->decoder.len_to_read) {
                memcpy(s->rx_buf + s->decoder.data_len, data, data_len);
                ret = data_len;
                s->decoder.data_len += data_len;
                s->decoder.len_to_read -= data_len;
            } else {
                memcpy(s->rx_buf + s->decoder.data_len, data, s->decoder.len_to_read);
                ret = s->decoder.len_to_read;
                s->decoder.data_len += s->decoder.len_to_read;
                recv_crc = lwtp_byte2_to_uint16(s->rx_buf + s->decoder.data_len - LWTP_CRC_LEN);
                calc_crc = lwtp_crc16(s->rx_buf, s->decoder.data_len - LWTP_CRC_LEN);
                if (recv_crc != calc_crc) {
                    LWTP_LOG("CRC error, should be 0x%04X, but received 0x%04X\n", calc_crc, recv_crc);
                    ret = -1;
                }
                s->decoder.step = DS_FINISH;
            }
            break;
        case DS_FINISH:
            break;
    }

    return ret;
}

/**
 * @brief lwtp 请求数据处理器
 * @param lwtp lwtp 服务器实例
 * @param s lwtp 会话实例
 * @param data 待处理的数据
 * @param data_len 待处理的数据长度
 * @param send_data 发送数据的回调函数
 * @return <0 解析出错或异常，0 正常
 */
int lwtp_server_session_recv_process(lwtp_server_t *lwtp, lwtp_server_session_t *s, uint8_t *data, uint32_t data_len,
                                     void (*send_data)(uint8_t *data, uint32_t len)) {
    uint8_t *ptr = data;
    int ret = -1;
    // 此处会循环处理数据
    while (data_len > 0) {
        ret = lwtp_server_session_recv_preprocess(s, ptr, data_len);
        if (ret >= 0) {
            ptr += ret;
            data_len -= ret;
        } else {
            // 出错处理,复位解码器
            s->decoder.step = DS_HEAD;
            s->decoder.data_len = 0;
            s->decoder.len_to_read = 1;
            // 此处默认逻辑为数据移位后重新解析，后续可能会拓展其他处理模式，如直接丢弃所有数据
            ptr++;
            data_len--;
        }

        if (s->decoder.step == DS_FINISH) {
            // 处理数据
            memset(&s->cpi, 0, sizeof(cmd_process_info_t));
            s->cpi.cmd = s->rx_buf[1];
            s->cpi.rx_data = s->rx_buf + LWTP_REQ_LEN_OFFSET + LWTP_REQ_LEN_SZ;
            s->cpi.rx_data_len = s->decoder.data_len - LWTP_REQ_INFO_LEN;
            s->cpi.tx_data_buf = s->tx_buf + LWTP_RSP_DATA_OFFSET;
            s->cpi.tx_data_buf_sz = LWTP_TX_BUF_SZ - LWTP_RSP_INFO_LEN;
            s->cpi.ack_status = LWTP_ACK_OK;
            s->cpi.ack_code = LWTP_ERR_CODE_NULL;

            ret = lwtp_server_cmd_process(lwtp, s);

            if (ret > 0 && send_data) {
                send_data(s->tx_buf, ret);
            }

            // 处理完成后，复位解码器
            s->decoder.step = DS_HEAD;
            s->decoder.data_len = 0;
            s->decoder.len_to_read = 1;

            ret = 0;
        }
    }

    return ret;
}

int lwtp_server_cmd_handler_register(lwtp_server_t *lwtp, cmd_handler_t *ch) {
    if (ch == NULL) {
        LWTP_LOG("cmd_handler is NULL\n");
        return -1;
    }
    if (ch->handler == NULL) {
        LWTP_LOG("cmd_handler->handler is NULL\n");
        return -1;
    }
    ch->next = NULL;

    if (lwtp->cmd_handler == NULL) {
        lwtp->cmd_handler = ch;
    } else {
        cmd_handler_t *tmp = lwtp->cmd_handler;
        while (tmp->next) tmp = tmp->next;
        tmp->next = ch;
    }

    return 0;
}

int lwtp_server_session_init(lwtp_server_session_t *s) {
    memset(s, 0, sizeof(lwtp_server_session_t));
    s->decoder.step = DS_HEAD;
    s->decoder.data_len = 0;
    s->decoder.len_to_read = 1;
    return 0;
}

int lwtp_server_init(lwtp_server_t *server) {
    memset(server, 0, sizeof(lwtp_server_t));
    return 0;
}
