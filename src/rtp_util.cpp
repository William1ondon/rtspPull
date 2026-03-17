#include "h264_util.h"

/*
 * Assemble one RTP payload into an Annex-B style H.264 byte stream buffer.
 *
 * Notes:
 * - This function assumes a fixed 12-byte RTP header (no CSRC/extension).
 * - It currently handles:
 *   1) FU-A (NAL type 28): reassembles fragmented NAL units.
 *   2) SPS/PPS (NAL type 7/8): writes start code + full NAL payload.
 * - Other NAL packetization modes are ignored by returning 0.
 *
 * Params:
 * - raw: full RTP packet bytes.
 * - frameBuffer: destination Annex-B buffer.
 * - bufOffset: current write offset in frameBuffer.
 * - maxBufferSize: capacity of frameBuffer.
 * - payloadLen: RTP payload size (excluding RTP header).
 *
 * Return:
 * - New buffer offset when bytes are appended successfully.
 * - 0 when payload is invalid/unsupported or buffer space is insufficient.
 */
int rtp_assemble_h264_frame(const uint8_t* raw, uint8_t* frameBuffer, uint32_t bufOffset, uint32_t maxBufferSize, uint32_t payloadLen)
{
    const uint8_t* payload;

    // RTP fixed header is assumed to be 12 bytes.
    payload = raw + 12;
    // Reserve room for potential Annex-B start code before writing payload.
    if (payloadLen <= 0 || (bufOffset + payloadLen + H264_START_CODE_LEN) > maxBufferSize) {
        return 0;
    }

    // NAL type is in the low 5 bits of the first payload byte.
    if ((raw[12] & 0x1F) == 28) {
        // FU-A: rebuild original NAL header and append fragment payload.
        bufOffset = assemble_fu_a(frameBuffer, bufOffset, payload, payloadLen);
    } else if ((raw[12] & 0x1F) == 7 || (raw[12] & 0x1F) == 8) {
        // SPS/PPS: write as complete NAL with Annex-B start code.
        WRITE_H264_START_CODE(frameBuffer + bufOffset);
        bufOffset += H264_START_CODE_LEN;

        memcpy(frameBuffer + bufOffset, payload, payloadLen);
        bufOffset += payloadLen;
    } else {
        // Unsupported NAL packet type in current implementation.
        return 0;
    }
    
    return bufOffset;
}