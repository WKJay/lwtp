#ifndef LWTP_UTILS_H
#define LWTP_UTILS_H
#include <stdint.h>
#include <stddef.h>

uint16_t lwtp_crc16(uint8_t *buffer, uint16_t buffer_length);
uint16_t lwtp_byte2_to_uint16(uint8_t *p);

#endif  // LWTP_UTILS_H
