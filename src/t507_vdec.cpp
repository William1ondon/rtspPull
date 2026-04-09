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
#include "framework/libcedarc/include/veInterface.h"

#define ALIGN_16B(x) (((x) + 15) & ~15)

namespace {
constexpr size_t kRetainedInputSafetyCount = 256;
constexpr size_t kRetainedInputMaxBytes = 128 * 1024 * 1024;
constexpr size_t kDisplayHoldCount = 2;

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
        if (verbose)
        {
            std::printf("[vdec-extbuf] chn=%d sync ve context skipped decoderCtx=%p engine=%p\n",
                        chn,
                        reinterpret_cast<void*>(decoderCtx),
                        decoderCtx != nullptr ? reinterpret_cast<void*>(decoderCtx->pVideoEngine) : nullptr);
        }
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
        std::printf("[vdec-extbuf] chn=%d sync ve context cfg(%p,%p) <- engine(%p,%p)\n",
                    chn,
                    reinterpret_cast<void*>(cfgOps),
                    cfgSelf,
                    reinterpret_cast<void*>(engineOps),
                    engineSelf);
    }
    else if (verbose)
    {
        std::printf("[vdec-extbuf] chn=%d sync ve context unchanged cfg(%p,%p)\n",
                    chn,
                    reinterpret_cast<void*>(cfgOps),
                    cfgSelf);
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
      m_perfDecodeNestedCallbackUsMax(0)
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

    const double elapsedMs = static_cast<double>(elapsedUs) / 1000.0;
    const double decodeAvgMs = m_perfDecodeCalls > 0 ? static_cast<double>(m_perfDecodeUsTotal) / static_cast<double>(m_perfDecodeCalls) / 1000.0 : 0.0;
    const double decodeMaxMs = static_cast<double>(m_perfDecodeUsMax) / 1000.0;
    const double decodeOnlyAvgMs = m_perfDecodeCalls > 0 ? static_cast<double>(m_perfDecodeOnlyUsTotal) / static_cast<double>(m_perfDecodeCalls) / 1000.0 : 0.0;
    const double decodeOnlyMaxMs = static_cast<double>(m_perfDecodeOnlyUsMax) / 1000.0;
    const double inDecodeCbAvgMs = m_perfDecodeCalls > 0 ? static_cast<double>(m_perfDecodeNestedCallbackUsTotal) / static_cast<double>(m_perfDecodeCalls) / 1000.0 : 0.0;
    const double inDecodeCbMaxMs = static_cast<double>(m_perfDecodeNestedCallbackUsMax) / 1000.0;
    const double cbAvgMs = m_perfCallbackCalls > 0 ? static_cast<double>(m_perfCallbackUsTotal) / static_cast<double>(m_perfCallbackCalls) / 1000.0 : 0.0;
    const double cbMaxMs = static_cast<double>(m_perfCallbackUsMax) / 1000.0;
    const double copyAvgMs = m_perfCallbackCalls > 0 ? static_cast<double>(m_perfCopyUsTotal) / static_cast<double>(m_perfCallbackCalls) / 1000.0 : 0.0;
    const double copyMaxMs = static_cast<double>(m_perfCopyUsMax) / 1000.0;
    const double syncAvgMs = m_perfCallbackCalls > 0 ? static_cast<double>(m_perfSyncUsTotal) / static_cast<double>(m_perfCallbackCalls) / 1000.0 : 0.0;
    const double syncMaxMs = static_cast<double>(m_perfSyncUsMax) / 1000.0;

    std::printf("[vdec-prof] chn=%d extbuf=1 elapsed_ms=%.1f decode=%lu fail=%lu decode_avg=%.3f decode_max=%.3f decode_only_avg=%.3f decode_only_max=%.3f in_decode_cb_avg=%.3f in_decode_cb_max=%.3f cb=%lu cb_in=%lu cb_out=%lu cb_avg=%.3f cb_max=%.3f copy_avg=%.3f copy_max=%.3f sync_avg=%.3f sync_max=%.3f\n",
                m_chn,
                elapsedMs,
                m_perfDecodeCalls,
                m_perfDecodeFails,
                decodeAvgMs,
                decodeMaxMs,
                decodeOnlyAvgMs,
                decodeOnlyMaxMs,
                inDecodeCbAvgMs,
                inDecodeCbMaxMs,
                m_perfCallbackCalls,
                m_perfCallbackInsideDecodeCalls,
                m_perfCallbackOutsideDecodeCalls,
                cbAvgMs,
                cbMaxMs,
                copyAvgMs,
                copyMaxMs,
                syncAvgMs,
                syncMaxMs);

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

int t507_vdec_node::registerExternalBuffers()
{
    FbmBufInfo* info = GetVideoFbmBufInfo(m_decoder);
    if (info == nullptr)
    {
        std::printf("[vdec-extbuf] chn=%d fbm info not ready yet\n", m_chn);
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
        std::printf("[vdec-extbuf] chn=%d invalid fbm buffer count 0\n", m_chn);
        return -1;
    }
    if (requiredBuffers > availableBuffers)
    {
        std::printf("[vdec-extbuf] chn=%d insufficient external buffers need=%zu have=%zu\n",
                    m_chn,
                    requiredBuffers,
                    availableBuffers);
        return -1;
    }

    const size_t yPlaneSize = static_cast<size_t>(m_bufferWidth) * static_cast<size_t>(m_bufferHeight);
    const size_t cPlaneSize = yPlaneSize / 2;
    m_outputBuffers.clear();
    m_outputBuffers.resize(requiredBuffers);
    m_registeredBufferCount = 0;

    std::printf("[vdec-extbuf] chn=%d fbm num=%d buf=%dx%d align=%d fmt=%d crop(l=%d,t=%d,r=%d,b=%d)\n",
                m_chn,
                m_fbmBufInfo.nBufNum,
                m_fbmBufInfo.nBufWidth,
                m_fbmBufInfo.nBufHeight,
                m_fbmBufInfo.nAlignValue,
                m_fbmBufInfo.ePixelFormat,
                m_fbmBufInfo.nLeftOffset,
                m_fbmBufInfo.nTopOffset,
                m_fbmBufInfo.nRightOffset,
                m_fbmBufInfo.nBottomOffset);

    for (size_t i = 0; i < requiredBuffers; ++i)
    {
        ion_mem mem{};
        if (my_buffer::getInstance()->getVideobuffer(m_chn, static_cast<int>(i), &mem) != 0)
        {
            std::printf("[vdec-extbuf] chn=%d failed to fetch video buffer idx=%zu\n", m_chn, i);
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
        out.picture.pPrivate = nullptr;

        VideoDecoderContextShadow* decoderCtx = getDecoderContextShadow(m_decoder);
        VeOpsS* veOps = nullptr;
        void* veSelf = nullptr;
        if (decoderCtx != nullptr)
        {
            if (decoderCtx->pVideoEngine != nullptr && decoderCtx->pVideoEngine->veOpsS != nullptr)
            {
                veOps = decoderCtx->pVideoEngine->veOpsS;
                veSelf = decoderCtx->pVideoEngine->pVeOpsSelf;
            }
            else
            {
                veOps = decoderCtx->vconfig.veOpsS;
                veSelf = decoderCtx->vconfig.pVeOpsSelf;
            }
        }
        if (veOps != nullptr && veSelf != nullptr && out.mem.dmafd >= 0)
        {
            struct user_iommu_param iommuBuf{};
            iommuBuf.fd = out.mem.dmafd;
            const int iommuRet = CdcVeGetIommuAddr(veOps, veSelf, &iommuBuf);
            if (iommuRet == 0 && iommuBuf.iommu_addr != 0)
            {
                const unsigned int phyOffset = CdcVeGetPhyOffset(veOps, veSelf);
                out.picture.phyYBufAddr = static_cast<size_addr>(iommuBuf.iommu_addr - phyOffset);
                out.picture.phyCBufAddr = static_cast<size_addr>(out.picture.phyYBufAddr + yPlaneSize);
                std::printf("[vdec-extbuf] chn=%d idx=%zu iommu=0x%lx phyOff=0x%x y=0x%lx c=0x%lx fd=%d\n",
                            m_chn,
                            i,
                            static_cast<unsigned long>(iommuBuf.iommu_addr),
                            phyOffset,
                            static_cast<unsigned long>(out.picture.phyYBufAddr),
                            static_cast<unsigned long>(out.picture.phyCBufAddr),
                            out.mem.dmafd);
            }
            else
            {
                std::printf("[vdec-extbuf] chn=%d idx=%zu iommu map failed ret=%d, fallback phy y=0x%lx c=0x%lx fd=%d\n",
                            m_chn,
                            i,
                            iommuRet,
                            static_cast<unsigned long>(out.picture.phyYBufAddr),
                            static_cast<unsigned long>(out.picture.phyCBufAddr),
                            out.mem.dmafd);
            }
        }
        else
        {
            std::printf("[vdec-extbuf] chn=%d idx=%zu iommu skipped veOps=%p veSelf=%p fd=%d\n",
                        m_chn,
                        i,
                        reinterpret_cast<void*>(veOps),
                        veSelf,
                        out.mem.dmafd);
        }

        std::printf("[vdec-extbuf] chn=%d idx=%zu virt=%p phy=0x%lx c=0x%lx fd=%d size=%d\n",
                    m_chn,
                    i,
                    reinterpret_cast<void*>(out.mem.virt),
                    static_cast<unsigned long>(out.picture.phyYBufAddr),
                    static_cast<unsigned long>(out.picture.phyCBufAddr),
                    out.mem.dmafd,
                    out.picture.nBufSize);

        VideoPicture* registered = SetVideoFbmBufAddress(m_decoder, &out.picture, 0);
        if (registered == nullptr)
        {
            std::printf("[vdec-extbuf] chn=%d SetVideoFbmBufAddress failed idx=%zu\n", m_chn, i);
            return -1;
        }

        out.registered = true;
        ++m_registeredBufferCount;
    }

    std::printf("[vdec-extbuf] chn=%d registered %zu external fbm buffers\n", m_chn, m_registeredBufferCount);
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
        std::printf("[vdec-extbuf] chn=%d ReopenVideoEngine gpu=%d failed ret=%d\n",
                    m_chn,
                    enable ? 1 : 0,
                    ret);
        return -1;
    }

    m_videoConf = reopenedConf;
    m_gpuBufferModeEnabled = enable;
    syncDecoderVeContext(m_decoder, &m_videoConf, m_chn, true);
    std::printf("[vdec-extbuf] chn=%d reopened decoder gpu=%d\n", m_chn, enable ? 1 : 0);
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

    const int releaseRet = SetVideoFbmBufRelease(m_decoder);
    if (releaseRet != 0)
    {
        std::printf("[vdec-extbuf] chn=%d SetVideoFbmBufRelease failed ret=%d\n", m_chn, releaseRet);
        return -1;
    }

    const size_t yPlaneSize = static_cast<size_t>(m_bufferWidth) * static_cast<size_t>(m_bufferHeight);
    size_t recycled = 0;
    for (size_t guard = 0; guard < m_registeredBufferCount; ++guard)
    {
        VideoPicture* released = RequestReleasePicture(m_decoder);
        if (released == nullptr)
        {
            break;
        }

        ExternalOutputBuffer* out = findOutputBuffer(released);
        if (out == nullptr)
        {
            std::printf("[vdec-extbuf] chn=%d release queue returned unknown buffer id=%d pic=%p\n",
                        m_chn,
                        released->nBufId,
                        released);
            if (ReturnRelasePicture(m_decoder, released, 0) == nullptr)
            {
                return -1;
            }
            ++recycled;
            continue;
        }

        released->ePixelFormat = m_fbmBufInfo.ePixelFormat;
        released->nWidth = m_bufferWidth;
        released->nHeight = m_bufferHeight;
        released->nLineStride = m_bufferWidth;
        released->nTopOffset = m_fbmBufInfo.nTopOffset;
        released->nBottomOffset = m_fbmBufInfo.nBottomOffset;
        released->nLeftOffset = m_fbmBufInfo.nLeftOffset;
        released->nRightOffset = m_fbmBufInfo.nRightOffset;
        released->pData0 = reinterpret_cast<char*>(out->mem.virt);
        released->pData1 = reinterpret_cast<char*>(out->mem.virt + yPlaneSize);
        released->pData2 = released->pData1 + yPlaneSize / 4;
        released->nBufFd = out->mem.dmafd;
        released->nBufStatus = 0;
        released->pPrivate = reinterpret_cast<void*>(out->mem.virt);

        std::printf("[vdec-extbuf] chn=%d ReturnRelasePicture begin idx=%d fd=%d\n", m_chn, released->nBufId, released->nBufFd);
        std::fflush(stdout);
        VideoPicture* requeued = ReturnRelasePicture(m_decoder, released, 0);
        if (requeued == nullptr)
        {
            std::printf("[vdec-extbuf] chn=%d ReturnRelasePicture failed idx=%d\n", m_chn, out->picture.nBufId);
            return -1;
        }

        ++recycled;
        std::printf("[vdec-extbuf] chn=%d recycle idx=%d y=0x%lx c=0x%lx fd=%d\n",
                    m_chn,
                    requeued->nBufId,
                    static_cast<unsigned long>(requeued->phyYBufAddr),
                    static_cast<unsigned long>(requeued->phyCBufAddr),
                    requeued->nBufFd);
        std::fflush(stdout);
    }

    if (recycled == 0)
    {
        std::printf("[vdec-extbuf] chn=%d release queue not ready yet\n", m_chn);
        return 1;
    }

    m_externalReleaseModeArmed = true;
    std::printf("[vdec-extbuf] chn=%d armed release mode recycled=%zu\n", m_chn, recycled);
    std::fflush(stdout);
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
    if (ret != 0)
    {
        std::printf("[vdec-extbuf] chn=%d ReturnPicture failed ret=%d pic=%p id=%d buf=%d\n",
                    m_chn,
                    ret,
                    picture,
                    picture->nID,
                    picture->nBufId);
    }
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

    for (int guard = 0; guard < 64; ++guard)
    {
        const int validNum = ValidPictureNum(m_decoder, 0);
        if (validNum <= 0)
        {
            break;
        }

        VideoPicture* picture = RequestPicture(m_decoder, 0);
        if (picture == nullptr)
        {
            break;
        }

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
        std::printf("[vdec-extbuf] chn=%d display picture missing private buffer binding\n", m_chn);
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
        std::printf("[vdec-extbuf] chn=%d allocOpen failed ret=%d\n", m_chn, ret);
        return -1;
    }
    m_memOpsOpened = true;

    std::memset(&m_streamInfo, 0, sizeof(m_streamInfo));
    std::memset(&m_videoConf, 0, sizeof(m_videoConf));
    std::memset(&m_fbmBufInfo, 0, sizeof(m_fbmBufInfo));

    m_videoConf.memops = reinterpret_cast<struct ScMemOpsS*>(m_memopsParam.ops);
    if (m_videoConf.memops == nullptr)
    {
        std::printf("[vdec-extbuf] chn=%d memops is null\n", m_chn);
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
        std::printf("[vdec-extbuf] chn=%d CreateVideoDecoder failed\n", m_chn);
        destroy();
        return -1;
    }

    ret = InitializeVideoDecoder(m_decoder, &m_streamInfo, &m_videoConf);
    if (ret != 0)
    {
        std::printf("[vdec-extbuf] chn=%d InitializeVideoDecoder failed ret=%d\n", m_chn, ret);
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
    return 0;
}

int t507_vdec_node::sendFrame(media_frame* frame)
{
    if (!m_bCreated || m_decoder == nullptr)
    {
        std::printf("[vdec-extbuf] chn=%d decoder not created\n", m_chn);
        return -1;
    }
    if (frame == nullptr)
    {
        return -1;
    }
    if (frame->getPayloadType() != MEDIA_PT_H264)
    {
        std::printf("[vdec-extbuf] chn=%d payload must be H264\n", m_chn);
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
        std::printf("[vdec-extbuf] chn=%d bitstream buffer too small input=%u valid=%d\n", m_chn, inputLen, validSize);
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
        std::printf("[vdec-extbuf] chn=%d RequestVideoStreamBuffer failed ret=%d input=%u valid=%d\n",
                    m_chn,
                    ret,
                    inputLen,
                    validSize);
        return -1;
    }

    if ((packetBufLen + packetRingBufLen) < static_cast<int>(inputLen) || packetBuf == nullptr)
    {
        std::printf("[vdec-extbuf] chn=%d invalid stream buffer packet=%d ring=%d input=%u\n",
                    m_chn,
                    packetBufLen,
                    packetRingBufLen,
                    inputLen);
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
        std::printf("[vdec-extbuf] chn=%d SubmitVideoStreamData failed ret=%d len=%u\n", m_chn, ret, inputLen);
        return -1;
    }

    const uint64_t decodeStartUs = monotonicTimeUs();
    ret = DecodeVideoStream(m_decoder, 0, 0, 0, 0);
    const uint64_t decodeEndUs = monotonicTimeUs();
    const uint64_t decodeElapsedUs = decodeEndUs - decodeStartUs;

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
            std::printf("[vdec-extbuf] chn=%d try switch to external buffers attempt=%lu frameType=%s count=%lu\n",
                        m_chn,
                        m_gpuBufferSwitchAttempts,
                        getFrameTypeName(ft),
                        m_count);
            if (reopenDecoderWithGpuBuffers(true) == 0)
            {
                const int registerRet = registerExternalBuffers();
                if (registerRet == 0)
                {
                    m_externalReleaseModeArmed = true;
                    std::printf("[vdec-extbuf] chn=%d external buffers installed after reopen, stay on RequestPicture/ReturnPicture\n", m_chn);
                    std::fflush(stdout);
                    return 0;
                }

                std::printf("[vdec-extbuf] chn=%d register after reopen failed ret=%d, fallback to internal buffers\n",
                            m_chn,
                            registerRet);
                reopenDecoderWithGpuBuffers(false);
            }
            return 0;
        }
    }

    if (m_gpuBufferModeEnabled && m_registeredBufferCount == 0)
    {
        const int registerRet = registerExternalBuffers();
        if (registerRet == 0)
        {
            m_externalReleaseModeArmed = true;
            std::printf("[vdec-extbuf] chn=%d external buffers installed, defer release mode and stay on RequestPicture/ReturnPicture\n", m_chn);
            std::fflush(stdout);

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
        else if (registerRet > 0)
        {
            return 0;
        }
        else
        {
            std::printf("[vdec-extbuf] chn=%d registerExternalBuffers hard failed ret=%d\n", m_chn, registerRet);
            return -1;
        }
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

        if (m_decodeFailStreak == 1 || (m_decodeFailStreak % 20) == 0)
        {
            std::printf("[vdec-fail] chn=%d ret=%d streak=%u total=%lu frameType=%s inputLen=%u retained=%zu retainedBytes=%zu\n",
                        m_chn,
                        ret,
                        m_decodeFailStreak,
                        m_decodeFailCount,
                        getFrameTypeName(ft),
                        inputLen,
                        retainedCount,
                        retainedBytes);
        }
        return -1;
    }

    if (m_decodeFailStreak > 0)
    {
        std::printf("[vdec-recover] chn=%d streak=%u total=%lu frameType=%s inputLen=%u\n",
                    m_chn,
                    m_decodeFailStreak,
                    m_decodeFailCount,
                    getFrameTypeName(ft),
                    inputLen);
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






