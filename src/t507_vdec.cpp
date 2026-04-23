#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include <unistd.h>

#include "t507_vdec.h"
#include "common.h"
#include "framework/libcedarc/base/include/CdcUtil.h"
#include "framework/libcedarc/include/veAdapter.h"
#include "framework/libcedarc/include/veInterface.h"

#define ALIGN_16B(x) (((x) + 15) & ~15)

namespace {
constexpr size_t kRetainedInputSafetyCount = 256;
constexpr size_t kRetainedInputMaxBytes = 128 * 1024 * 1024;
constexpr size_t kDisplayHoldCount = 2;
constexpr bool kVdecStateTraceEnabled = false;
constexpr uint8_t kStartCode[4] = {0x00, 0x00, 0x00, 0x01};

static bool appendToDecoderStreamBuffer(const uint8_t* src,
                                        size_t len,
                                        char*& packetBuf,
                                        int& packetBufLen,
                                        char*& packetRingBuf,
                                        int& packetRingBufLen)
{
    if (len == 0)
    {
        return true;
    }
    if (src == nullptr)
    {
        return false;
    }

    while (len > 0)
    {
        if (packetBufLen > 0 && packetBuf != nullptr)
        {
            const size_t copyLen = std::min(len, static_cast<size_t>(packetBufLen));
            std::memcpy(packetBuf, src, copyLen);
            packetBuf += copyLen;
            packetBufLen -= static_cast<int>(copyLen);
            src += copyLen;
            len -= copyLen;
            continue;
        }

        if (packetRingBufLen > 0 && packetRingBuf != nullptr)
        {
            const size_t copyLen = std::min(len, static_cast<size_t>(packetRingBufLen));
            std::memcpy(packetRingBuf, src, copyLen);
            packetRingBuf += copyLen;
            packetRingBufLen -= static_cast<int>(copyLen);
            src += copyLen;
            len -= copyLen;
            continue;
        }

        return false;
    }

    return true;
}

template <typename... Args>
void vdecStateTrace(const char* fmt, Args... args)
{
    (void)fmt;
    (void)sizeof...(args);
}

template <typename... Args>
void vdecStateTraceFlush(const char* fmt, Args... args)
{
    (void)fmt;
    (void)sizeof...(args);
}

struct DecoderInterfaceShadow;

struct VideoEngineShadow
{
    VConfig                 vconfig;
    VideoStreamInfo         videoStreamInfo;
    void*                   pLibHandle;
    DecoderInterfaceShadow* pDecoderInterface;
    int                     bIsSoftwareDecoder;
    VideoFbmInfo            fbmInfo;
    u8                      nResetVeMode;
    VeOpsS*                 veOpsS;
    void*                   pVeOpsSelf;
};

struct VideoDecoderContextShadow
{
    VConfig             vconfig;
    VideoStreamInfo     videoStreamInfo;
    VideoEngineShadow*  pVideoEngine;
};

static VideoDecoderContextShadow* getDecoderContextShadow(VideoDecoder* decoder)
{
    return reinterpret_cast<VideoDecoderContextShadow*>(decoder);
}

static bool syncDecoderVeContext(VideoDecoder* decoder, VConfig* videoConf, int chn, bool verbose)
{
    VideoDecoderContextShadow* decoderCtx = getDecoderContextShadow(decoder);
    if (decoderCtx == nullptr || decoderCtx->pVideoEngine == nullptr)
    {
        return false;
    }

    VeOpsS* engineOps = decoderCtx->pVideoEngine->veOpsS;
    void* engineSelf = decoderCtx->pVideoEngine->pVeOpsSelf;
    VeOpsS* cfgOps = decoderCtx->vconfig.veOpsS;
    void* cfgSelf = decoderCtx->vconfig.pVeOpsSelf;

    if (engineOps != cfgOps || engineSelf != cfgSelf)
    {
        decoderCtx->vconfig.veOpsS = engineOps;
        decoderCtx->vconfig.pVeOpsSelf = engineSelf;
        if (videoConf != nullptr)
        {
            videoConf->veOpsS = engineOps;
            videoConf->pVeOpsSelf = engineSelf;
        }
    }

    return engineOps != nullptr && engineSelf != nullptr;
}

static uint64_t monotonicTimeUs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

static void accumulatePerfDuration(uint64_t elapsedUs, uint64_t& totalUs, uint64_t& maxUs)
{
    totalUs += elapsedUs;
    if (elapsedUs > maxUs)
    {
        maxUs = elapsedUs;
    }
}

static bool findStartCode(const uint8_t* data, size_t len, size_t* scLen)
{
    if (len >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
    {
        *scLen = 4;
        return true;
    }
    if (len >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1)
    {
        *scLen = 3;
        return true;
    }
    return false;
}

static unsigned int readUe(unsigned char* pBuff, unsigned int nLen, unsigned int& nStartBit)
{
    unsigned int nZeroNum = 0;
    while (nStartBit < nLen * 8)
    {
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            break;
        }
        ++nZeroNum;
        ++nStartBit;
    }
    ++nStartBit;

    unsigned long dwRet = 0;
    for (unsigned int i = 0; i < nZeroNum; ++i)
    {
        dwRet <<= 1;
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        ++nStartBit;
    }
    return (1 << nZeroNum) - 1 + dwRet;
}

static FrameType parseH264FrameType(const uint8_t* data, size_t len)
{
    size_t scLen = 0;
    if (!findStartCode(data, len, &scLen) || len <= scLen)
    {
        return FRAME_UNKNOWN;
    }

    const uint8_t nalType = data[scLen] & 0x1f;
    if (nalType == 5)
    {
        return FRAME_I;
    }
    if (nalType != 1 || len <= scLen + 1)
    {
        return FRAME_UNKNOWN;
    }

    const uint8_t* payload = data + scLen + 1;
    size_t payloadLen = len - scLen - 1;
    std::vector<uint8_t> rbsp;
    rbsp.reserve(payloadLen);

    for (size_t i = 0; i < payloadLen; ++i)
    {
        if (i + 2 < payloadLen && payload[i] == 0x00 && payload[i + 1] == 0x00 && payload[i + 2] == 0x03)
        {
            rbsp.push_back(0x00);
            rbsp.push_back(0x00);
            i += 2;
            continue;
        }
        rbsp.push_back(payload[i]);
    }

    if (rbsp.empty())
    {
        return FRAME_UNKNOWN;
    }

    unsigned int bit = 0;
    readUe(rbsp.data(), static_cast<unsigned int>(rbsp.size()), bit);
    const unsigned int sliceType = readUe(rbsp.data(), static_cast<unsigned int>(rbsp.size()), bit);

    switch (sliceType % 5)
    {
        case 2:
            return FRAME_I;
        case 0:
            return FRAME_P;
        case 1:
            return FRAME_B;
        default:
            return FRAME_UNKNOWN;
    }
}
} // namespace

t507_vdec_node::t507_vdec_node(int chn)
    : m_bCreated(false),
      m_memOpsOpened(false),
      m_chn(chn),
      m_decoder(nullptr),
      m_bOutEn(true),
      m_count(0),
      m_width(IMAGEWIDTH),
      m_height(IMAGEHEIGHT),
      m_bufferWidth(IMAGEWIDTH),
      m_bufferHeight(ALIGN_16B(IMAGEHEIGHT)),
      m_currentFrameIndex(-1),
      m_gpuBufferModeEnabled(false),
      m_gpuBufferSwitchAttempts(0),
      m_iommuVeOps(nullptr),
      m_iommuVeSelf(nullptr),
      m_registeredBufferCount(0),
      m_externalReleaseModeArmed(false),
      m_pendingPicture(nullptr),
      m_displayPicture(nullptr),
      m_retainedInputBytes(0),
      m_decodeFailStreak(0),
      m_decodeFailCount(0),
      m_perfWindowStartUs(0),
      m_perfDecodeCalls(0),
      m_perfDecodeFails(0),
      m_perfDecodeUsTotal(0),
      m_perfDecodeUsMax(0),
      m_perfCallbackCalls(0),
      m_perfCallbackUsTotal(0),
      m_perfCallbackUsMax(0),
      m_perfCopyUsTotal(0),
      m_perfCopyUsMax(0),
      m_perfSyncUsTotal(0),
      m_perfSyncUsMax(0),
      m_perfCallbackInsideDecodeCalls(0),
      m_perfCallbackOutsideDecodeCalls(0),
      m_perfDecodeOnlyUsTotal(0),
      m_perfDecodeOnlyUsMax(0),
      m_perfDecodeNestedCallbackUsTotal(0),
      m_perfDecodeNestedCallbackUsMax(0),
      m_noDisplayPicturePolls(0),
      m_requestedPictureCount(0),
      m_promotedPictureCount(0)
{
    std::memset(&m_streamInfo, 0, sizeof(m_streamInfo));
    std::memset(&m_videoConf, 0, sizeof(m_videoConf));
    std::memset(&m_memopsParam, 0, sizeof(m_memopsParam));
    std::memset(&m_fbmBufInfo, 0, sizeof(m_fbmBufInfo));
}

t507_vdec_node::~t507_vdec_node()
{
    destroy();
}

bool t507_vdec_node::isCreated()
{
    return m_bCreated;
}

char* t507_vdec_node::getFrameTypeName(FrameType t)
{
    switch (t)
    {
        case FRAME_I:
            return const_cast<char*>("I");
        case FRAME_P:
            return const_cast<char*>("P");
        case FRAME_B:
            return const_cast<char*>("B");
        default:
            return const_cast<char*>("U");
    }
}

void t507_vdec_node::maybeLogPerfWindowLocked(uint64_t nowUs, bool force)
{
    if (m_perfWindowStartUs == 0)
    {
        m_perfWindowStartUs = nowUs;
    }

    const uint64_t elapsedUs = nowUs - m_perfWindowStartUs;
    if (!force && elapsedUs < 1000000ULL)
    {
        return;
    }

    m_perfWindowStartUs = nowUs;
    m_perfDecodeCalls = 0;
    m_perfDecodeFails = 0;
    m_perfDecodeUsTotal = 0;
    m_perfDecodeUsMax = 0;
    m_perfCallbackCalls = 0;
    m_perfCallbackUsTotal = 0;
    m_perfCallbackUsMax = 0;
    m_perfCopyUsTotal = 0;
    m_perfCopyUsMax = 0;
    m_perfSyncUsTotal = 0;
    m_perfSyncUsMax = 0;
    m_perfCallbackInsideDecodeCalls = 0;
    m_perfCallbackOutsideDecodeCalls = 0;
    m_perfDecodeOnlyUsTotal = 0;
    m_perfDecodeOnlyUsMax = 0;
    m_perfDecodeNestedCallbackUsTotal = 0;
    m_perfDecodeNestedCallbackUsMax = 0;
}

bool t507_vdec_node::ensureIommuMapper()
{
    if (CdcIonGetMemType() != MEMORY_IOMMU)
    {
        return false;
    }
    if (m_iommuVeOps != nullptr && m_iommuVeSelf != nullptr)
    {
        return true;
    }

    m_iommuVeOps = GetVeOpsS(VE_OPS_TYPE_NORMAL);
    if (m_iommuVeOps == nullptr)
    {
        return false;
    }

    VeConfig veConfig{};
    veConfig.nDecoderFlag = 1;
    veConfig.bNotSetVeFreq = 1;
    m_iommuVeSelf = CdcVeInit(m_iommuVeOps, &veConfig);
    if (m_iommuVeSelf == nullptr)
    {
        m_iommuVeOps = nullptr;
        return false;
    }

    return true;
}

void t507_vdec_node::releaseIommuMapper()
{
    if (m_iommuVeOps != nullptr && m_iommuVeSelf != nullptr)
    {
        CdcVeRelease(m_iommuVeOps, m_iommuVeSelf);
    }
    m_iommuVeOps = nullptr;
    m_iommuVeSelf = nullptr;
}

int t507_vdec_node::registerExternalBuffers()
{
    FbmBufInfo* info = GetVideoFbmBufInfo(m_decoder);
    if (info == nullptr)
    {
        return 1;
    }

    m_fbmBufInfo = *info;
    m_bufferWidth = m_fbmBufInfo.nBufWidth;
    m_bufferHeight = m_fbmBufInfo.nBufHeight;
    syncDecoderVeContext(m_decoder, &m_videoConf, m_chn, true);

    const size_t availableBuffers = static_cast<size_t>(T507_PREVIEW_BUF_NUM + T507_PLAYBACK_BUF_NUM);
    const size_t requiredBuffers = static_cast<size_t>(m_fbmBufInfo.nBufNum);
    if (requiredBuffers == 0)
    {
        return -1;
    }
    if (requiredBuffers > availableBuffers)
    {
        return -1;
    }

    const size_t yPlaneSize = static_cast<size_t>(m_bufferWidth) * static_cast<size_t>(m_bufferHeight);
    const size_t cPlaneSize = yPlaneSize / 2;
    m_outputBuffers.clear();
    m_outputBuffers.resize(requiredBuffers);
    m_registeredBufferCount = 0;

    for (size_t i = 0; i < requiredBuffers; ++i)
    {
        ion_mem mem{};
        if (my_buffer::getInstance()->getVideobuffer(m_chn, static_cast<int>(i), &mem) != 0)
        {
            return -1;
        }

        ExternalOutputBuffer& out = m_outputBuffers[i];
        out.mem = mem;
        out.frame = std::make_unique<frame_shell>();
        std::memset(&out.picture, 0, sizeof(out.picture));
        out.picture.ePixelFormat = m_fbmBufInfo.ePixelFormat;
        out.picture.nWidth = m_fbmBufInfo.nBufWidth;
        out.picture.nHeight = m_fbmBufInfo.nBufHeight;
        out.picture.nLineStride = m_fbmBufInfo.nBufWidth;
        out.picture.nTopOffset = m_fbmBufInfo.nTopOffset;
        out.picture.nBottomOffset = m_fbmBufInfo.nBottomOffset;
        out.picture.nLeftOffset = m_fbmBufInfo.nLeftOffset;
        out.picture.nRightOffset = m_fbmBufInfo.nRightOffset;
        out.picture.pData0 = reinterpret_cast<char*>(out.mem.virt);
        out.picture.pData1 = reinterpret_cast<char*>(out.mem.virt + yPlaneSize);
        out.picture.pData2 = nullptr;
        out.picture.phyYBufAddr = static_cast<size_addr>(out.mem.phy);
        out.picture.phyCBufAddr = static_cast<size_addr>(out.mem.phy + yPlaneSize);
        out.picture.nBufId = static_cast<int>(i);
        out.picture.nBufFd = out.mem.dmafd;
        out.picture.nBufSize = static_cast<int>(yPlaneSize + cPlaneSize);
        out.picture.nBufStatus = 0;
        out.metadata.assign(4 * 1024, 0);
        out.picture.pData3 = nullptr;
        out.picture.pPrivate = reinterpret_cast<void*>(out.mem.virt);
        out.picture.pMetaData = out.metadata.empty() ? nullptr : out.metadata.data();

        VeOpsS* veOps = m_videoConf.veOpsS;
        void* veSelf = m_videoConf.pVeOpsSelf;
        if ((veOps == nullptr || veSelf == nullptr) && CdcIonGetMemType() == MEMORY_IOMMU)
        {
            if (ensureIommuMapper())
            {
                veOps = m_iommuVeOps;
                veSelf = m_iommuVeSelf;
            }
        }
        if (veOps != nullptr && veSelf != nullptr && out.mem.dmafd >= 0 && CdcIonGetMemType() == MEMORY_IOMMU)
        {
            struct user_iommu_param iommuBuf{};
            iommuBuf.fd = out.mem.dmafd;
            const int iommuRet = CdcVeGetIommuAddr(veOps, veSelf, &iommuBuf);
            if (iommuRet == 0 && iommuBuf.iommu_addr != 0)
            {
                const unsigned int phyOffset = CdcVeGetPhyOffset(veOps, veSelf);
                out.picture.phyYBufAddr = static_cast<size_addr>(iommuBuf.iommu_addr - phyOffset);
                out.picture.phyCBufAddr = static_cast<size_addr>(out.picture.phyYBufAddr + yPlaneSize);
            }
            else
            {
            }
        }

        VideoPicture* registered = SetVideoFbmBufAddress(m_decoder, &out.picture, 0);
        if (registered == nullptr)
        {
            return -1;
        }

        out.registered = true;
        ++m_registeredBufferCount;
    }

    return 0;
}

int t507_vdec_node::reopenDecoderWithGpuBuffers(bool enable)
{
    if (m_decoder == nullptr)
    {
        return -1;
    }

    returnDecoderPicture(m_pendingPicture);
    returnDecoderPicture(m_displayPicture);
    while (!m_displayHoldQueue.empty())
    {
        VideoPicture* picture = m_displayHoldQueue.front();
        m_displayHoldQueue.pop_front();
        returnDecoderPicture(picture);
    }

    m_outputBuffers.clear();
    m_registeredBufferCount = 0;
    m_gpuBufferModeEnabled = false;
    m_gpuBufferSwitchAttempts = 0;
    m_externalReleaseModeArmed = false;
    m_currentFrameIndex = -1;

    VConfig reopenedConf = m_videoConf;
    reopenedConf.bGpuBufValid = enable ? 1 : 0;
    const int ret = ReopenVideoEngine(m_decoder, &reopenedConf, &m_streamInfo);
    if (ret != 0)
    {
        return -1;
    }

    m_videoConf = reopenedConf;
    m_gpuBufferModeEnabled = enable;
    syncDecoderVeContext(m_decoder, &m_videoConf, m_chn, true);
    return 0;
}

 t507_vdec_node::ExternalOutputBuffer* t507_vdec_node::findOutputBuffer(VideoPicture* picture)
{
    if (picture == nullptr)
    {
        return nullptr;
    }

    const int idx = picture->nBufId;
    if (idx < 0 || idx >= static_cast<int>(m_outputBuffers.size()))
    {
        return nullptr;
    }

    ExternalOutputBuffer& out = m_outputBuffers[static_cast<size_t>(idx)];
    return out.registered ? &out : nullptr;
}

const t507_vdec_node::ExternalOutputBuffer* t507_vdec_node::findOutputBuffer(const VideoPicture* picture) const
{
    if (picture == nullptr)
    {
        return nullptr;
    }

    const int idx = picture->nBufId;
    if (idx < 0 || idx >= static_cast<int>(m_outputBuffers.size()))
    {
        return nullptr;
    }

    const ExternalOutputBuffer& out = m_outputBuffers[static_cast<size_t>(idx)];
    return out.registered ? &out : nullptr;
}

int t507_vdec_node::armExternalBufferReleaseMode()
{

    if (m_decoder == nullptr || m_registeredBufferCount == 0)
    {
        return -1;
    }
    if (m_externalReleaseModeArmed)
    {
        return 0;
    }

    // In gpu-buffer mode, SetVideoFbmBufAddress() already pushes each registered
    // FrameNode into FBM's empty queue. We only need to verify that FBM sees the
    // external pool; forcing the "new display release" path here crashes inside
    // the vendor wrapper RequestReleasePicture() on this platform.
    const int totalNum = TotalPictureBufferNum(m_decoder, 0);
    if (totalNum <= 0)
    {
        return 1;
    }

    const int emptyNum = EmptyPictureBufferNum(m_decoder, 0);
    const int validNum = ValidPictureNum(m_decoder, 0);

    m_externalReleaseModeArmed = true;
    return 0;
}

void t507_vdec_node::returnDecoderPicture(VideoPicture*& picture)
{
    if (picture == nullptr || m_decoder == nullptr)
    {
        picture = nullptr;
        return;
    }

    const int ret = ReturnPicture(m_decoder, picture);
    (void)ret;
    picture = nullptr;
}

void t507_vdec_node::releaseHeldPictures()
{
    while (m_displayHoldQueue.size() > kDisplayHoldCount)
    {
        VideoPicture* picture = m_displayHoldQueue.front();
        m_displayHoldQueue.pop_front();
        returnDecoderPicture(picture);
    }
}

void t507_vdec_node::drainDecodedPictures()
{
    if (m_decoder == nullptr)
    {
        return;
    }

    if (!m_gpuBufferModeEnabled || m_registeredBufferCount == 0 || !m_externalReleaseModeArmed)
    {
        return;
    }

    for (int guard = 0; guard < 64; ++guard)
    {
        const int validNum = ValidPictureNum(m_decoder, 0);
        if (validNum <= 0)
        {
            ++m_noDisplayPicturePolls;
            (void)validNum;
            break;
        }
        m_noDisplayPicturePolls = 0;

        VideoPicture* picture = RequestPicture(m_decoder, 0);
        if (picture == nullptr)
        {
            break;
        }
        ++m_requestedPictureCount;

        if (m_pendingPicture != nullptr)
        {
            VideoPicture* stale = m_pendingPicture;
            m_pendingPicture = nullptr;
            returnDecoderPicture(stale);
        }

        m_pendingPicture = picture;
    }
}

bool t507_vdec_node::promotePendingPicture()
{
    if (m_pendingPicture == nullptr)
    {
        return false;
    }

    if (m_displayPicture != nullptr)
    {
        m_displayHoldQueue.push_back(m_displayPicture);
    }

    m_displayPicture = m_pendingPicture;
    m_pendingPicture = nullptr;
    releaseHeldPictures();

    ExternalOutputBuffer* out = findOutputBuffer(m_displayPicture);
    if (out == nullptr)
    {
        returnDecoderPicture(m_displayPicture);
        m_currentFrameIndex = -1;
        return false;
    }

    const unsigned int visibleWidth = m_displayPicture->nWidth > 0 ? static_cast<unsigned int>(m_displayPicture->nWidth) : static_cast<unsigned int>(m_width);
    const unsigned int visibleHeight = m_displayPicture->nHeight > 0 ? static_cast<unsigned int>(m_displayPicture->nHeight) : static_cast<unsigned int>(m_height);
    const bool keyFrame = (m_displayPicture->nCurFrameInfo.enVidFrmType == VIDEO_FORMAT_TYPE_I ||
                           m_displayPicture->nCurFrameInfo.enVidFrmType == VIDEO_FORMAT_TYPE_IDR);

    out->frame->refill(MEDIA_PT_YUV_420SP_NV21,
                       reinterpret_cast<void*>(out->mem.virt),
                       out->mem.phy,
                       visibleWidth,
                       visibleHeight,
                       static_cast<unsigned long long>(m_displayPicture->nPts),
                       keyFrame);
    m_currentFrameIndex = m_displayPicture->nBufId;
    ++m_promotedPictureCount;
    return true;
}

int t507_vdec_node::create()
{
    if (m_bCreated)
    {
        return 0;
    }

    int ret = allocOpen(MEM_TYPE_CDX_NEW, &m_memopsParam, nullptr);
    if (ret < 0)
    {
        return -1;
    }
    m_memOpsOpened = true;

    /*
        表示原始视频流的参数结构体，
        包含编码格式、宽高、帧率、时长、纵横比、3D/安全流标志、帧/流包标记
        以及 codec 特定数据指针等字段。
    */
    std::memset(&m_streamInfo, 0, sizeof(m_streamInfo));
    
    /*
        表示一个用于视频解码器/视频处理配置的结构体，
        包含缩放、旋转、输出格式、缓冲区、硬件加速、保护、内存操作和其他解码控制标志等
        一系列配置选项
    */
    std::memset(&m_videoConf, 0, sizeof(m_videoConf));
    
    /*
        表示新显示用的缓冲区信息，
        包含缓冲区编号、宽高、像素格式、对齐值、各种标志位以及上下左右偏移量。
    */
    std::memset(&m_fbmBufInfo, 0, sizeof(m_fbmBufInfo));

    m_videoConf.memops = reinterpret_cast<struct ScMemOpsS*>(m_memopsParam.ops);
    if (m_videoConf.memops == nullptr)
    {
        destroy();
        return -1;
    }

    m_streamInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
    m_streamInfo.nWidth = m_width;
    m_streamInfo.nHeight = m_height;

    m_videoConf.eOutputPixelFormat = PIXEL_FORMAT_NV21;
    m_videoConf.nDisplayHoldingFrameBufferNum = 1;
    m_videoConf.nDecodeSmoothFrameBufferNum = 3;
    m_videoConf.nVeFreq = 300 * 1000 * 1000;
    m_videoConf.bIsFullFramePerPiece = 1;
    m_videoConf.bGpuBufValid = 0;

    m_decoder = CreateVideoDecoder();
    if (m_decoder == nullptr)
    {
        destroy();
        return -1;
    }

    ret = InitializeVideoDecoder(m_decoder, &m_streamInfo, &m_videoConf);
    if (ret != 0)
    {
        destroy();
        return -1;
    }

    syncDecoderVeContext(m_decoder, &m_videoConf, m_chn, true);

    m_bCreated = true;
    m_decodeFailStreak = 0;
    m_gpuBufferModeEnabled = false;
    m_gpuBufferSwitchAttempts = 0;
    m_externalReleaseModeArmed = false;
    m_currentFrameIndex = -1;
    m_noDisplayPicturePolls = 0;
    m_requestedPictureCount = 0;
    m_promotedPictureCount = 0;
    {
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        m_perfWindowStartUs = 0;
        m_perfDecodeCalls = 0;
        m_perfDecodeFails = 0;
        m_perfDecodeUsTotal = 0;
        m_perfDecodeUsMax = 0;
        m_perfCallbackCalls = 0;
        m_perfCallbackUsTotal = 0;
        m_perfCallbackUsMax = 0;
        m_perfCopyUsTotal = 0;
        m_perfCopyUsMax = 0;
        m_perfSyncUsTotal = 0;
        m_perfSyncUsMax = 0;
        m_perfCallbackInsideDecodeCalls = 0;
        m_perfCallbackOutsideDecodeCalls = 0;
        m_perfDecodeOnlyUsTotal = 0;
        m_perfDecodeOnlyUsMax = 0;
        m_perfDecodeNestedCallbackUsTotal = 0;
        m_perfDecodeNestedCallbackUsMax = 0;
    }

    return 0;
}

int t507_vdec_node::destroy()
{
    if (m_decoder != nullptr)
    {
        returnDecoderPicture(m_pendingPicture);
        returnDecoderPicture(m_displayPicture);
        while (!m_displayHoldQueue.empty())
        {
            VideoPicture* picture = m_displayHoldQueue.front();
            m_displayHoldQueue.pop_front();
            returnDecoderPicture(picture);
        }

        DestroyVideoDecoder(m_decoder);
        m_decoder = nullptr;
    }

    if (m_memOpsOpened)
    {
        allocClose(MEM_TYPE_CDX_NEW, &m_memopsParam, nullptr);
        m_memOpsOpened = false;
        std::memset(&m_memopsParam, 0, sizeof(m_memopsParam));
    }

    {
        std::lock_guard<std::mutex> lock(m_retainedInputsMutex);
        m_retainedInputs.clear();
        m_retainedInputBytes = 0;
    }

    m_outputBuffers.clear();
    m_registeredBufferCount = 0;
    m_externalReleaseModeArmed = false;
    m_currentFrameIndex = -1;
    m_bCreated = false;
    m_pendingPicture = nullptr;
    m_displayPicture = nullptr;
    m_noDisplayPicturePolls = 0;
    m_requestedPictureCount = 0;
    m_promotedPictureCount = 0;
    releaseIommuMapper();
    return 0;
}

int t507_vdec_node::sendFrame(media_frame* frame)
{
    if (!m_bCreated || m_decoder == nullptr)
    {
        return -1;
    }
    if (frame == nullptr)
    {
        return -1;
    }
    if (frame->getPayloadType() != MEDIA_PT_H264)
    {
        return -1;
    }

    void* inputVir = nullptr;
    frame->lockPacket(0, &inputVir, nullptr);
    if (inputVir == nullptr)
    {
        return -1;
    }

    const unsigned int inputLen = frame->getPacketSize(0);
    if (inputLen == 0)
    {
        return -1;
    }

    const FrameType ft = parseH264FrameType(static_cast<const uint8_t*>(inputVir), inputLen);

    const int validSize = VideoStreamBufferSize(m_decoder, 0) - VideoStreamDataSize(m_decoder, 0);
    if (static_cast<int>(inputLen) > validSize)
    {
        return -1;
    }

    char* packetBuf = nullptr;
    int packetBufLen = 0;
    char* packetRingBuf = nullptr;
    int packetRingBufLen = 0;
    int ret = RequestVideoStreamBuffer(m_decoder,
                                       static_cast<int>(inputLen),
                                       &packetBuf,
                                       &packetBufLen,
                                       &packetRingBuf,
                                       &packetRingBufLen,
                                       0);
    if (ret != 0)
    {
        return -1;
    }

    if ((packetBufLen + packetRingBufLen) < static_cast<int>(inputLen) || packetBuf == nullptr)
    {
        return -1;
    }

    if (inputLen <= static_cast<unsigned int>(packetBufLen))
    {
        std::memcpy(packetBuf, inputVir, inputLen);
    }
    else
    {
        std::memcpy(packetBuf, inputVir, packetBufLen);
        std::memcpy(packetRingBuf,
                    static_cast<const uint8_t*>(inputVir) + packetBufLen,
                    inputLen - static_cast<unsigned int>(packetBufLen));
    }

    VideoStreamDataInfo dataInfo;
    std::memset(&dataInfo, 0, sizeof(dataInfo));
    dataInfo.nID = static_cast<int>(++m_count);
    dataInfo.pData = packetBuf;
    dataInfo.nLength = static_cast<int>(inputLen);
    dataInfo.nPts = frame->getPts() > 0 ? static_cast<int64_t>(frame->getPts()) : static_cast<int64_t>(monotonicTimeUs());
    dataInfo.nPcr = dataInfo.nPts;
    dataInfo.bIsFirstPart = 1;
    dataInfo.bIsLastPart = 1;
    dataInfo.bValid = 1;

    ret = SubmitVideoStreamData(m_decoder, &dataInfo, 0);
    if (ret != 0)
    {
        return -1;
    }

    return decodeSubmittedFrame(ft, inputLen);
}

int t507_vdec_node::sendH264FrameDirect(const uint8_t* nalData,
                                        size_t nalSize,
                                        const std::vector<uint8_t>& sps,
                                        const std::vector<uint8_t>& pps,
                                        bool isIDR,
                                        long long pts)
{
    if (!m_bCreated || m_decoder == nullptr || nalData == nullptr || nalSize == 0)
    {
        return -1;
    }

    const bool needExtra = isIDR && !sps.empty() && !pps.empty();
    size_t inputLen = sizeof(kStartCode) + nalSize;
    if (needExtra)
    {
        inputLen += sizeof(kStartCode) + sps.size();
        inputLen += sizeof(kStartCode) + pps.size();
    }

    if (inputLen > static_cast<size_t>(0x7fffffff))
    {
        return -1;
    }

    const int validSize = VideoStreamBufferSize(m_decoder, 0) - VideoStreamDataSize(m_decoder, 0);
    if (static_cast<int>(inputLen) > validSize)
    {
        return -1;
    }

    char* packetBuf = nullptr;
    int packetBufLen = 0;
    char* packetRingBuf = nullptr;
    int packetRingBufLen = 0;
    int ret = RequestVideoStreamBuffer(m_decoder,
                                       static_cast<int>(inputLen),
                                       &packetBuf,
                                       &packetBufLen,
                                       &packetRingBuf,
                                       &packetRingBufLen,
                                       0);
    if (ret != 0)
    {
        return -1;
    }

    if ((packetBufLen + packetRingBufLen) < static_cast<int>(inputLen) || packetBuf == nullptr)
    {
        return -1;
    }

    char* packetStart = packetBuf;
    if (needExtra)
    {
        if (!appendToDecoderStreamBuffer(kStartCode, sizeof(kStartCode), packetBuf, packetBufLen, packetRingBuf, packetRingBufLen) ||
            !appendToDecoderStreamBuffer(sps.data(), sps.size(), packetBuf, packetBufLen, packetRingBuf, packetRingBufLen) ||
            !appendToDecoderStreamBuffer(kStartCode, sizeof(kStartCode), packetBuf, packetBufLen, packetRingBuf, packetRingBufLen) ||
            !appendToDecoderStreamBuffer(pps.data(), pps.size(), packetBuf, packetBufLen, packetRingBuf, packetRingBufLen))
        {
            return -1;
        }
    }

    if (!appendToDecoderStreamBuffer(kStartCode, sizeof(kStartCode), packetBuf, packetBufLen, packetRingBuf, packetRingBufLen) ||
        !appendToDecoderStreamBuffer(nalData, nalSize, packetBuf, packetBufLen, packetRingBuf, packetRingBufLen))
    {
        return -1;
    }

    VideoStreamDataInfo dataInfo;
    std::memset(&dataInfo, 0, sizeof(dataInfo));
    dataInfo.nID = static_cast<int>(++m_count);
    dataInfo.pData = packetStart;
    dataInfo.nLength = static_cast<int>(inputLen);
    dataInfo.nPts = pts > 0 ? static_cast<int64_t>(pts) : static_cast<int64_t>(monotonicTimeUs());
    dataInfo.nPcr = dataInfo.nPts;
    dataInfo.bIsFirstPart = 1;
    dataInfo.bIsLastPart = 1;
    dataInfo.bValid = 1;

    ret = SubmitVideoStreamData(m_decoder, &dataInfo, 0);
    if (ret != 0)
    {
        return -1;
    }

    return decodeSubmittedFrame(isIDR ? FRAME_I : FRAME_P, static_cast<unsigned int>(inputLen));
}

int t507_vdec_node::decodeSubmittedFrame(FrameType ft, unsigned int inputLen)
{
    const uint64_t decodeStartUs = monotonicTimeUs();

    /*
        负责处理视频流解码的整个过程，
        包括是否为流结束、是否仅解码关键帧、是否丢弃 B 帧以减少延迟，以及当前时间戳。
        函数返回解码结果，表示解码状态
    */
    int ret = DecodeVideoStream(m_decoder, 0, 0, 0, 0);
    
    const uint64_t decodeEndUs = monotonicTimeUs();
    const uint64_t decodeElapsedUs = decodeEndUs - decodeStartUs;
    bool retryDecodeAfterArm = false;

    {
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        ++m_perfDecodeCalls;
        accumulatePerfDuration(decodeElapsedUs, m_perfDecodeUsTotal, m_perfDecodeUsMax);
        accumulatePerfDuration(decodeElapsedUs, m_perfDecodeOnlyUsTotal, m_perfDecodeOnlyUsMax);
        if (!(ret == VDECODE_RESULT_KEYFRAME_DECODED ||
              ret == VDECODE_RESULT_FRAME_DECODED ||
              ret == VDECODE_RESULT_NO_BITSTREAM ||
              ret == VDECODE_RESULT_CONTINUE))
        {
            ++m_perfDecodeFails;
        }
        maybeLogPerfWindowLocked(decodeEndUs);
    }

    if (!m_gpuBufferModeEnabled)
    {
        FbmBufInfo* info = GetVideoFbmBufInfo(m_decoder);
        if (info != nullptr && (ft == FRAME_I || m_count > 30))
        {
            ++m_gpuBufferSwitchAttempts;
            if (reopenDecoderWithGpuBuffers(true) == 0)
            {
                const int registerRet = registerExternalBuffers();
                if (registerRet == 0)
                {
                }
                else if (registerRet > 0)
                {
                    return 0;
                }
                else
                {
                    reopenDecoderWithGpuBuffers(false);
                    return 0;
                }
            }
        }
    }

    if (m_gpuBufferModeEnabled && m_registeredBufferCount == 0)
    {
        const int registerRet = registerExternalBuffers();
        if (registerRet == 0)
        {
        }
        else if (registerRet > 0)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }

    if (m_gpuBufferModeEnabled && m_registeredBufferCount > 0 && !m_externalReleaseModeArmed)
    {
        const int armRet = armExternalBufferReleaseMode();
        if (armRet > 0)
        {
            return 0;
        }
        if (armRet < 0)
        {
            return -1;
        }

        retryDecodeAfterArm = true;
    }

    if (retryDecodeAfterArm)
    {
        const uint64_t decodeRetryStartUs = monotonicTimeUs();
        ret = DecodeVideoStream(m_decoder, 0, 0, 0, 0);
        const uint64_t decodeRetryEndUs = monotonicTimeUs();
        const uint64_t decodeRetryElapsedUs = decodeRetryEndUs - decodeRetryStartUs;
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        ++m_perfDecodeCalls;
        accumulatePerfDuration(decodeRetryElapsedUs, m_perfDecodeUsTotal, m_perfDecodeUsMax);
        accumulatePerfDuration(decodeRetryElapsedUs, m_perfDecodeOnlyUsTotal, m_perfDecodeOnlyUsMax);
        if (!(ret == VDECODE_RESULT_KEYFRAME_DECODED ||
              ret == VDECODE_RESULT_FRAME_DECODED ||
              ret == VDECODE_RESULT_NO_BITSTREAM ||
              ret == VDECODE_RESULT_CONTINUE))
        {
            ++m_perfDecodeFails;
        }
        maybeLogPerfWindowLocked(decodeRetryEndUs);
    }

    if (!(ret == VDECODE_RESULT_KEYFRAME_DECODED ||
          ret == VDECODE_RESULT_FRAME_DECODED ||
          ret == VDECODE_RESULT_NO_BITSTREAM ||
          ret == VDECODE_RESULT_CONTINUE ||
          ret == VDECODE_RESULT_NO_FRAME_BUFFER))
    {
        ++m_decodeFailStreak;
        ++m_decodeFailCount;

        size_t retainedCount = 0;
        size_t retainedBytes = 0;
        {
            std::lock_guard<std::mutex> lock(m_retainedInputsMutex);
            retainedCount = m_retainedInputs.size();
            retainedBytes = m_retainedInputBytes;
        }

        (void)retainedCount;
        (void)retainedBytes;
        return -1;
    }

    if (m_decodeFailStreak > 0)
    {
        m_decodeFailStreak = 0;
    }

    drainDecodedPictures();
    return 0;
}

void t507_vdec_node::retainInputBuffer(const std::shared_ptr<std::vector<uint8_t>>& buffer)
{
    if (!buffer)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_retainedInputsMutex);
    m_retainedInputs.push_back(buffer);
    m_retainedInputBytes += buffer->size();

    while (m_retainedInputBytes > kRetainedInputMaxBytes && m_retainedInputs.size() > kRetainedInputSafetyCount)
    {
        m_retainedInputBytes -= m_retainedInputs.front()->size();
        m_retainedInputs.pop_front();
    }
}

media_frame* t507_vdec_node::getFrame()
{
    if (!m_gpuBufferModeEnabled || m_registeredBufferCount == 0 || !m_externalReleaseModeArmed)
    {
        return nullptr;
    }

    if (!promotePendingPicture())
    {
        return nullptr;
    }

    if (m_currentFrameIndex < 0 || m_currentFrameIndex >= static_cast<int>(m_outputBuffers.size()))
    {
        return nullptr;
    }

    return m_outputBuffers[static_cast<size_t>(m_currentFrameIndex)].frame.get();
}



