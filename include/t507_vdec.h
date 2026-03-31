#pragma once

#include "AWVideoDecoder.h"
#include "my_buffer.h"
#include "my_log.h"
#include "media_frame.h"
#include "DmaIon.h"
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

using namespace awvideodecoder;

typedef enum FrameType_
{
    FRAME_UNKNOWN = 0,
    FRAME_I,
    FRAME_P,
    FRAME_B
}FrameType;

class t507_vdec_node : public AWVideoDecoderDataCallback
{
private:
    bool m_bCreated;
    int m_chn;
    AWVideoDecoder *m_decoder;
    bool m_bOutEn;
    unsigned long m_count;
    int m_width;
    int m_height;

    frame_shell *m_frame[T507_PLAYBACK_BUF_NUM];
    ion_mem mem;

    int m_curFrame;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> m_retainedInputs;
    std::mutex m_retainedInputsMutex;
    size_t m_retainedInputBytes;
    unsigned int m_decodeFailStreak;
    unsigned long m_decodeFailCount;
    std::mutex m_perfMutex;
    uint64_t m_perfWindowStartUs;
    unsigned long m_perfDecodeCalls;
    unsigned long m_perfDecodeFails;
    uint64_t m_perfDecodeUsTotal;
    uint64_t m_perfDecodeUsMax;
    unsigned long m_perfCallbackCalls;
    uint64_t m_perfCallbackUsTotal;
    uint64_t m_perfCallbackUsMax;
    uint64_t m_perfCopyUsTotal;
    uint64_t m_perfCopyUsMax;
    uint64_t m_perfSyncUsTotal;
    uint64_t m_perfSyncUsMax;
    unsigned long m_perfCallbackInsideDecodeCalls;
    unsigned long m_perfCallbackOutsideDecodeCalls;
    uint64_t m_perfDecodeOnlyUsTotal;
    uint64_t m_perfDecodeOnlyUsMax;
    uint64_t m_perfDecodeNestedCallbackUsTotal;
    uint64_t m_perfDecodeNestedCallbackUsMax;
    bool m_decodeTimingActive;
    uint64_t m_decodeTimingCallbackUs;
    bool m_bypassCopyProbe;
    unsigned long m_bypassCopyFrameCount;
    bool m_useG2dCopy;
    int m_g2dHandle;
    unsigned long m_perfG2dCalls;
    unsigned long m_perfG2dFails;
    uint64_t m_perfG2dUsTotal;
    uint64_t m_perfG2dUsMax;

public:
    // t507_vdec_node(int chn, const MEDIA_VDEC_ATTR *attr);
    t507_vdec_node(int chn);
    ~t507_vdec_node();

    AWVideoDecoder *getDecoder();

    /* AWVideoDecoderDataCallback */
    int decoderDataReady(awvideodecoder::AVPacket *packet);

    /* base node */
    bool isCreated();
    int create();
    int destroy();
    // media_node *getNode();

    /* input */
    // int getInputId();
    // /* vdec没有实现getFrame 直接调用sendFrame就是开启数据解码 */
    int sendFrame(media_frame *frame);
    void retainInputBuffer(const std::shared_ptr<std::vector<uint8_t>>& buffer);
    // bool isBound();
    // // media_output *getBindObject();
    // bool unbind();

    /* output */
    // int getOutputId();
    // bool isEnable();
    // int enable();
    // int disable();
    // int getBindCnt();
    // int getBoundObject(media_input **inputArray);
    // bool bind(media_input *input);
    // bool unbind(media_input *input);
    media_frame *getFrame();
    // int releaseFrame(int seq);

private:
    char *getFrameTypeName(FrameType t);
    void maybeLogPerfWindowLocked(uint64_t nowUs, bool force = false);
};

