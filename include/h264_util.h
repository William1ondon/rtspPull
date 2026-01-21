#ifndef H264_UTIL_H
#define H264_UTIL_H

#include <string.h>
#include <stdint.h>

#define H264_START_CODE_LEN 4

static const uint8_t H264_START_CODE[H264_START_CODE_LEN] = {0x00, 0x00, 0x00, 0x01};

#define WRITE_H264_START_CODE(ptr) \
    memcpy((ptr), H264_START_CODE, H264_START_CODE_LEN)


uint32_t assemble_fu_a(uint8_t* frameBuffer, uint32_t bufOffset, const uint8_t* pkt, int pkt_size);

#endif /* H264_UTIL_H */