#pragma once

#include "framework/libcedarc/include/vdecoder.h"
#include "framework/libcedarc/include/veInterface.h"
#include "sunxiMemInterface.h"
#include "my_buffer.h"
#include "my_log.h"
#include "media_frame.h"
#include "DmaIon.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

typedef enum FrameType_
{
    FRAME_UNKNOWN = 0,
    FRAME_I,
    FRAME_P,
    FRAME_B
} FrameType;

class t507_vdec_node
{
private:
    struct ExternalOutputBuffer
    {
        ion_mem mem{};
        VideoPicture picture{};
        std::unique_ptr<frame_shell> frame;
        std::vector<unsigned char> metadata;
        bool registered = false;
    };

    bool m_bCreated;
    bool m_memOpsOpened;
    int m_chn;
    VideoDecoder* m_decoder;
    bool m_bOutEn;
    unsigned long m_count;
    int m_width;
    int m_height;
    int m_bufferWidth;
    int m_bufferHeight;
    int m_currentFrameIndex;
    bool m_gpuBufferModeEnabled;
    unsigned long m_gpuBufferSwitchAttempts;
    VeOpsS* m_iommuVeOps;
    void* m_iommuVeSelf;

    VideoStreamInfo m_streamInfo;
    VConfig m_videoConf;
    paramStruct_t m_memopsParam;
    FbmBufInfo m_fbmBufInfo;

    std::vector<ExternalOutputBuffer> m_outputBuffers;
    size_t m_registeredBufferCount;
    bool m_externalReleaseModeArmed;
    VideoPicture* m_pendingPicture;
    VideoPicture* m_displayPicture;
    std::deque<VideoPicture*> m_displayHoldQueue;

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
    unsigned long m_noDisplayPicturePolls;
    unsigned long m_requestedPictureCount;
    unsigned long m_promotedPictureCount;

public:
    explicit t507_vdec_node(int chn);
    ~t507_vdec_node();

    bool isCreated();
    int create();
    int destroy();
    
    // 还是原来的用法，会在内部进行必要的封装和拷贝，适用于输入数据不完整或者没有完整NALU数据的场景
    int sendFrame(media_frame* frame);

    // 直接往解码器输入NALU数据，绕过sendFrame接口的封装（可以少一次拷贝），适用于已经有完整NALU数据的场景
    int sendH264FrameDirect(const uint8_t* nalData,
                            size_t nalSize,
                            const std::vector<uint8_t>& sps,
                            const std::vector<uint8_t>& pps,
                            bool isIDR,
                            long long pts);
    void retainInputBuffer(const std::shared_ptr<std::vector<uint8_t>>& buffer);
    media_frame* getFrame();

private:
    char* getFrameTypeName(FrameType t);
    void maybeLogPerfWindowLocked(uint64_t nowUs, bool force = false);
    bool ensureIommuMapper();
    void releaseIommuMapper();
    int registerExternalBuffers();
    int reopenDecoderWithGpuBuffers(bool enable);
    int armExternalBufferReleaseMode();
    ExternalOutputBuffer* findOutputBuffer(VideoPicture* picture);
    const ExternalOutputBuffer* findOutputBuffer(const VideoPicture* picture) const;
    void releaseHeldPictures();
    void returnDecoderPicture(VideoPicture*& picture);
    void drainDecodedPictures();
    bool promotePendingPicture();
    int decodeSubmittedFrame(FrameType ft, unsigned int inputLen);
};
