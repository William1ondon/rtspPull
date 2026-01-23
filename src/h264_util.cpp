#include <stdio.h>
#include <stdint.h>
#include "h264_util.h"

uint32_t assemble_fu_a(uint8_t* frameBuffer, uint32_t bufOffset, const uint8_t* pkt, int pkt_size)
{
    const uint8_t* payload = pkt;
    uint8_t fu_indicator = payload[0];
    uint8_t fu_header = payload[1];

    uint8_t start = (fu_header & 0x80) >> 7;
    uint8_t end   = (fu_header & 0x40) >> 6;

    uint8_t nal_type = fu_header & 0x1F;
    uint8_t nri = fu_indicator & 0x60;
    uint8_t f_bit = fu_indicator & 0x80;

    uint8_t *fu_payload = (uint8_t*)payload + 2;
    int fu_payload_size = pkt_size - 2;

    if (start) {
        WRITE_H264_START_CODE(frameBuffer + bufOffset);
        bufOffset += H264_START_CODE_LEN;
        frameBuffer[bufOffset++] = f_bit | nri | nal_type;
    }
    
    memcpy(frameBuffer + bufOffset, fu_payload, fu_payload_size);
    bufOffset += fu_payload_size;

    return bufOffset;
}