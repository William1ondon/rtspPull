#include "h264_util.h"

int rtp_assemble_h264_frame(const uint8_t* raw, uint8_t* frameBuffer, uint32_t bufOffset, uint32_t maxBufferSize, uint32_t payloadLen)
{
    const uint8_t* payload;

    payload = raw + 12;
    if (payloadLen <= 0 || (bufOffset + payloadLen + H264_START_CODE_LEN) > maxBufferSize) {
        return 0;
    }

    if ((raw[12] & 0x1F) == 28) {
        bufOffset = assemble_fu_a(frameBuffer, bufOffset, payload, payloadLen);
    } else if ((raw[12] & 0x1F) == 7 || (raw[12] & 0x1F) == 8) {
        WRITE_H264_START_CODE(frameBuffer + bufOffset);
        bufOffset += H264_START_CODE_LEN;

        memcpy(frameBuffer + bufOffset, payload, payloadLen);
        bufOffset += payloadLen;
    } else {
        return 0;
    }
    
    return bufOffset;
}