#ifndef LWTP_USER_CFG_H
#define LWTP_USER_CFG_H

#define LWTP_DEBUG 1  // 是否开启调试
#if LWTP_DEBUG
#include <stdio.h>
#define LWTP_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define LWTP_LOG(fmt, ...)
#endif

#define LWTP_HEAD        0xE1  // 请求帧头

#define LWTP_RX_BUF_SZ 1024  // 接收缓冲区大小
#define LWTP_TX_BUF_SZ 1024  // 发送缓冲区大小

#define LWTP_REQ_HEAD_SZ 1     // 请求帧头大小

#define LWTP_REQ_LEN_OFFSET 2  // 请求帧的数据长度偏移
#define LWTP_REQ_LEN_SZ     2  // 请求帧的数据长度大小

#endif  // LWTP_USER_CFG_H
