#ifndef RTP_UTIL_H
#define RTP_UTIL_H

#include <stdint.h>

int rtp_assemble_h264_frame(const uint8_t* raw, uint8_t* frameBuffer, uint32_t bufOffset, uint32_t maxBufferSize, uint32_t payloadLen);

#endif /* RTP_UTIL_H */