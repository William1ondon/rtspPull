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
} FrameType;

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
    unsigned long m_perfDisplaySkipCount;
    bool m_bypassCopyProbe;
    unsigned long m_bypassCopyFrameCount;
    unsigned long m_displaySkipCount;
    bool m_dropDisplayForNextDecode;
    bool m_hasFreshFrame;

public:
    t507_vdec_node(int chn);
    ~t507_vdec_node();

    AWVideoDecoder *getDecoder();

    int decoderDataReady(awvideodecoder::AVPacket *packet);

    bool isCreated();
    int create();
    int destroy();

    int sendFrame(media_frame *frame);
    void setDisplayDropHint(bool drop);
    void retainInputBuffer(const std::shared_ptr<std::vector<uint8_t>>& buffer);

    media_frame *getFrame();

private:
    char *getFrameTypeName(FrameType t);
    void maybeLogPerfWindowLocked(uint64_t nowUs, bool force = false);
};