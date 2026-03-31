#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/vfs.h>
#include <pthread.h>
#include <error.h>
#include <errno.h>

#include "t507_vdec.h"
#include "g2d_driver_enh.h"
#include <sys/ioctl.h>

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

using namespace awvideodecoder;

namespace {
constexpr size_t kRetainedInputSafetyCount = 256;
constexpr size_t kRetainedInputMaxBytes = 128 * 1024 * 1024;

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
}

static void splitAddr64(uint64_t addr, __u32* low, __u32* high)
{
    if (low != nullptr)
    {
        *low = static_cast<__u32>(addr & 0xffffffffULL);
    }
    if (high != nullptr)
    {
        *high = static_cast<__u32>((addr >> 32) & 0xffffffffULL);
    }
}

static void initG2dImagePhy(g2d_image_enh* image,
                            g2d_fmt_enh format,
                            uint64_t yAddr,
                            uint64_t cAddr,
                            uint64_t vAddr,
                            __u32 width,
                            __u32 height,
                            __u32 clipW,
                            __u32 clipH)
{
    memset(image, 0, sizeof(*image));
    image->format = format;
    image->width = width;
    image->height = height;
    image->align[0] = 0;
    image->align[1] = 0;
    image->align[2] = 0;
    image->clip_rect.x = 0;
    image->clip_rect.y = 0;
    image->clip_rect.w = clipW;
    image->clip_rect.h = clipH;
    image->use_phy_addr = 1;

    splitAddr64(yAddr, &image->laddr[0], &image->haddr[0]);
    splitAddr64(cAddr, &image->laddr[1], &image->haddr[1]);
    splitAddr64(vAddr, &image->laddr[2], &image->haddr[2]);
}

static void initG2dImageFd(g2d_image_enh* image,
                           g2d_fmt_enh format,
                           int fd,
                           __u32 width,
                           __u32 height,
                           __u32 clipW,
                           __u32 clipH)
{
    memset(image, 0, sizeof(*image));
    image->format = format;
    image->width = width;
    image->height = height;
    image->align[0] = 0;
    image->align[1] = 0;
    image->align[2] = 0;
    image->clip_rect.x = 0;
    image->clip_rect.y = 0;
    image->clip_rect.w = clipW;
    image->clip_rect.h = clipH;
    image->fd = fd;
    image->use_phy_addr = 0;
}

static int g2dBlitNv21ToFd(int g2dFd,
                           uint64_t srcY,
                           uint64_t srcC,
                           __u32 srcWidth,
                           __u32 srcHeight,
                           __u32 srcCropW,
                           __u32 srcCropH,
                           int dstFd,
                           __u32 dstWidth,
                           __u32 dstHeight,
                           __u32 dstCropW,
                           __u32 dstCropH)
{
    if (g2dFd < 0 || srcY == 0 || srcC == 0 || dstFd < 0)
    {
        errno = EINVAL;
        return -1;
    }

    g2d_blt_h blitPara;
    memset(&blitPara, 0, sizeof(blitPara));
    blitPara.flag_h = G2D_ROT_0;

    initG2dImagePhy(&blitPara.src_image_h,
                    G2D_FORMAT_YUV420UVC_V1U1V0U0,
                    srcY,
                    srcC,
                    0,
                    srcWidth,
                    srcHeight,
                    srcCropW,
                    srcCropH);
    initG2dImageFd(&blitPara.dst_image_h,
                   G2D_FORMAT_YUV420UVC_V1U1V0U0,
                   dstFd,
                   dstWidth,
                   dstHeight,
                   dstCropW,
                   dstCropH);

    return ioctl(g2dFd, G2D_CMD_BITBLT_H, &blitPara);
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


unsigned int Ue(unsigned char *pBuff, unsigned int nLen, unsigned int &nStartBit)
{
    // 计算0bit的个数
    unsigned int nZeroNum = 0;
    while (nStartBit < nLen * 8)
    {
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:按位与，%取余
        {
            break;
        }
        nZeroNum++;
        nStartBit++;
    }
    nStartBit++;

    // 计算结果
    unsigned long dwRet = 0;
    for (unsigned int i = 0; i < nZeroNum; i++)
    {
        dwRet <<= 1;
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return (1 << nZeroNum) - 1 + dwRet;
}

int Se(unsigned char *pBuff, unsigned int nLen, unsigned int &nStartBit)
{
    int UeVal = Ue(pBuff, nLen, nStartBit);
    double k = UeVal;
    int nValue = ceil(k / 2); // ceil函数：ceil函数的作用是求不小于给定实数的最小整数。ceil(2)=ceil(1.2)=cei(1.5)=2.00
    if (UeVal % 2 == 0)
        nValue = -nValue;
    return nValue;
}

unsigned long u(unsigned int BitCount, unsigned char *buf, unsigned int &nStartBit)
{
    unsigned long dwRet = 0;
    for (unsigned int i = 0; i < BitCount; i++)
    {
        dwRet <<= 1;
        if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
        {
            dwRet += 1;
        }
        nStartBit++;
    }
    return dwRet;
}

// 仅用于区分 I/P/B，解析 slice_type
static FrameType parseH264FrameType(const uint8_t* data, size_t len)
{
    size_t scLen = 0;
    if (!findStartCode(data, len, &scLen)) return FRAME_UNKNOWN;
    if (len <= scLen) return FRAME_UNKNOWN;

    uint8_t nalHdr = data[scLen];
    uint8_t nalType = nalHdr & 0x1f;
    if (nalType == 5) return FRAME_I; // IDR

    if (nalType != 1) return FRAME_UNKNOWN; // 非 slice

    // 生成 RBSP（去掉 emulation_prevention_three_byte）
    const uint8_t* p = data + scLen + 1;
    size_t payloadLen = len - scLen - 1;
    std::vector<uint8_t> rbsp;
    rbsp.reserve(payloadLen);

    for (size_t i = 0; i < payloadLen; ++i)
    {
        if (i + 2 < payloadLen && p[i] == 0x00 && p[i + 1] == 0x00 && p[i + 2] == 0x03)
        {
            rbsp.push_back(0x00);
            rbsp.push_back(0x00);
            i += 2;
            continue;
        }
        rbsp.push_back(p[i]);
    }

    unsigned int bit = 0;
    Ue(rbsp.data(), rbsp.size(), bit); // first_mb_in_slice
    unsigned int slice_type = Ue(rbsp.data(), rbsp.size(), bit);

    switch (slice_type % 5)
    {
        case 2: return FRAME_I; // I
        case 0: return FRAME_P; // P
        case 1: return FRAME_B; // B
        default: return FRAME_UNKNOWN;
    }
}

int h264_decode_sps(unsigned char *buf, unsigned int nLen, int *width, int *height)
{
    unsigned int StartBit = 0;
    int profile_idc = u(8, buf, StartBit);
    int constraint_set0_flag = u(1, buf, StartBit); //(buf[1] & 0x80)>>7;
    int constraint_set1_flag = u(1, buf, StartBit); //(buf[1] & 0x40)>>6;
    int constraint_set2_flag = u(1, buf, StartBit); //(buf[1] & 0x20)>>5;
    int constraint_set3_flag = u(1, buf, StartBit); //(buf[1] & 0x10)>>4;
    int reserved_zero_4bits = u(4, buf, StartBit);
    int level_idc = u(8, buf, StartBit);
    int seq_parameter_set_id = Ue(buf, nLen, StartBit);

    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 144)
    {
        int chroma_format_idc = Ue(buf, nLen, StartBit);
        if (chroma_format_idc == 3)
            int residual_colour_transform_flag = u(1, buf, StartBit);
        int bit_depth_luma_minus8 = Ue(buf, nLen, StartBit);
        int bit_depth_chroma_minus8 = Ue(buf, nLen, StartBit);
        int qpprime_y_zero_transform_bypass_flag = u(1, buf, StartBit);
        int seq_scaling_matrix_present_flag = u(1, buf, StartBit);

        int seq_scaling_list_present_flag[8];
        if (seq_scaling_matrix_present_flag)
        {
            for (int i = 0; i < 8; i++)
                seq_scaling_list_present_flag[i] = u(1, buf, StartBit);
        }
    }

    int log2_max_frame_num_minus4 = Ue(buf, nLen, StartBit);
    int pic_order_cnt_type = Ue(buf, nLen, StartBit);
    if (pic_order_cnt_type == 0)
        int log2_max_pic_order_cnt_lsb_minus4 = Ue(buf, nLen, StartBit);
    else if (pic_order_cnt_type == 1)
    {
        int delta_pic_order_always_zero_flag = u(1, buf, StartBit);
        int offset_for_non_ref_pic = Se(buf, nLen, StartBit);
        int offset_for_top_to_bottom_field = Se(buf, nLen, StartBit);
        int num_ref_frames_in_pic_order_cnt_cycle = Ue(buf, nLen, StartBit);

        int *offset_for_ref_frame = new int[num_ref_frames_in_pic_order_cnt_cycle];
        for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
            offset_for_ref_frame[i] = Se(buf, nLen, StartBit);
        delete[] offset_for_ref_frame;
    }
    int num_ref_frames = Ue(buf, nLen, StartBit);
    int gaps_in_frame_num_value_allowed_flag = u(1, buf, StartBit);
    int pic_width_in_mbs_minus1 = Ue(buf, nLen, StartBit);
    int pic_height_in_map_units_minus1 = Ue(buf, nLen, StartBit);

    // width=(pic_width_in_mbs_minus1+1)*16;
    // height=(pic_height_in_map_units_minus1+1)*16;

    int frame_mbs_only_flag = u(1, buf, StartBit);
    if (!frame_mbs_only_flag)
        int mb_adaptive_frame_field_flag = u(1, buf, StartBit);

    int direct_8x8_inference_flag = u(1, buf, StartBit);
    int frame_cropping_flag = u(1, buf, StartBit);
    int frame_crop_left_offset = 0;
    int frame_crop_right_offset = 0;
    int frame_crop_top_offset = 0;
    int frame_crop_bottom_offset = 0;
    if (frame_cropping_flag)
    {
        frame_crop_left_offset = Ue(buf, nLen, StartBit);
        frame_crop_right_offset = Ue(buf, nLen, StartBit);
        frame_crop_top_offset = Ue(buf, nLen, StartBit);
        frame_crop_bottom_offset = Ue(buf, nLen, StartBit);
    }

    *width = ((pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_left_offset * 2 - frame_crop_right_offset * 2;
    *height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16) - (frame_crop_top_offset * 2) - (frame_crop_bottom_offset * 2);
    return true;
}

t507_vdec_node::t507_vdec_node(int chn)
{
    m_chn = chn;
    m_bCreated = false;

    m_decoder = nullptr;

    m_bOutEn = true;

    // m_width = 1280;
    // m_height = 720;

    m_width = IMAGEWIDTH;
    m_height = IMAGEHEIGHT;

    m_count = 0;

    m_curFrame = 0;
    m_retainedInputBytes = 0;
    m_decodeFailStreak = 0;
    m_decodeFailCount = 0;
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
    m_decodeTimingActive = false;
    m_decodeTimingCallbackUs = 0;
    const char *bypassEnv = getenv("RTSPPULL_BYPASS_VDEC_COPY");
    m_bypassCopyProbe = (bypassEnv != nullptr && bypassEnv[0] != '\0' && strcmp(bypassEnv, "0") != 0);
    m_bypassCopyFrameCount = 0;
    const char *g2dEnv = getenv("RTSPPULL_USE_G2D_COPY");
    m_useG2dCopy = !(g2dEnv != nullptr && g2dEnv[0] != '\0' && strcmp(g2dEnv, "0") == 0);
    m_g2dHandle = -1;
    m_perfG2dCalls = 0;
    m_perfG2dFails = 0;
    m_perfG2dUsTotal = 0;
    m_perfG2dUsMax = 0;
    if (m_bypassCopyProbe)
    {
        printf("[vdec-probe] chn=%d bypass copy/Ion enabled via RTSPPULL_BYPASS_VDEC_COPY\n", m_chn);
    }

    for (int i = 0; i < T507_PLAYBACK_BUF_NUM; i++)
    {
        /* 初始化帧数组 要用这些帧去装载v4l2数据 */
        m_frame[i] = new frame_shell();
    }

    // create();
}

t507_vdec_node::~t507_vdec_node()
{
}

char *t507_vdec_node::getFrameTypeName(FrameType t)
{
    switch (t)
    {
        case FRAME_I:
            return "I";
        case FRAME_P:
            return "P";
        case FRAME_B:
            return "B";
        default:
            return "U";
    }
}

int t507_vdec_node::create()
{
    int ret;
    DecodeParam param;
    memset(&param, 0, sizeof(DecodeParam));
    param.codecType = CODEC_H264;
    param.pixelFormat = PIXEL_NV21;
    param.srcW = m_width;  // If you don't know, set 0.
    param.srcH = m_height; // If you don't know, set 0.
    param.scaleRatio = ScaleNone;
    param.rotation = Angle_0;
    m_decoder = AWVideoDecoder::create();
    ret = m_decoder->init(&param, this);
    if (ret < 0 && ret > -100)
    // if (ret < 0)
    {
        logError("Decoder init fail:%d \n", ret);
        return -1;
    }
    else
        logInfo("decoder init success. \n");

    const char *g2dEnv = getenv("RTSPPULL_USE_G2D_COPY");
    printf("[vdec-g2d] chn=%d requested=%d env=%s\n",
           m_chn,
           m_useG2dCopy ? 1 : 0,
           g2dEnv != nullptr ? g2dEnv : "<unset>");
    fflush(stdout);

    if (m_useG2dCopy)
    {
        m_g2dHandle = open("/dev/g2d", O_RDWR | O_CLOEXEC);
        if (m_g2dHandle < 0)
        {
            m_useG2dCopy = false;
            printf("[vdec-g2d] chn=%d open /dev/g2d failed errno=%d(%s), fallback to memcpy\n",
                   m_chn,
                   errno,
                   strerror(errno));
            fflush(stdout);
        }
        else
        {
            printf("[vdec-g2d] chn=%d enabled fd=%d\n", m_chn, m_g2dHandle);
            fflush(stdout);
        }
    }
    else
    {
        printf("[vdec-g2d] chn=%d disabled by RTSPPULL_USE_G2D_COPY\n", m_chn);
        fflush(stdout);
    }

    m_bCreated = true;
    m_decodeFailStreak = 0;
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
        m_decodeTimingActive = false;
        m_decodeTimingCallbackUs = 0;
        m_perfG2dCalls = 0;
        m_perfG2dFails = 0;
        m_perfG2dUsTotal = 0;
        m_perfG2dUsMax = 0;
    }
    return 0;
}

int t507_vdec_node::destroy()
{
    if (m_decoder != NULL)
    {
        m_bCreated = false;
        usleep(100 * 1000); // 延时是让送过去的解码视频帧解完
        if (m_g2dHandle >= 0)
        {
            close(m_g2dHandle);
            m_g2dHandle = -1;
        }
        AWVideoDecoder::destroy(m_decoder);

        m_decoder = NULL;
        m_g2dHandle = -1;
    }
    else
        logWarn("vdec had been destoryed already. \n");

    {
        std::lock_guard<std::mutex> lock(m_retainedInputsMutex);
        m_retainedInputs.clear();
        m_retainedInputBytes = 0;
    }

    return 0;
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
    const double g2dAvgMs = m_perfG2dCalls > 0 ? static_cast<double>(m_perfG2dUsTotal) / static_cast<double>(m_perfG2dCalls) / 1000.0 : 0.0;
    const double g2dMaxMs = static_cast<double>(m_perfG2dUsMax) / 1000.0;

    printf("[vdec-prof] chn=%d bypass=%d g2d=%d elapsed_ms=%.1f decode=%lu fail=%lu decode_avg=%.3f decode_max=%.3f decode_only_avg=%.3f decode_only_max=%.3f in_decode_cb_avg=%.3f in_decode_cb_max=%.3f cb=%lu cb_in=%lu cb_out=%lu cb_avg=%.3f cb_max=%.3f copy_avg=%.3f copy_max=%.3f sync_avg=%.3f sync_max=%.3f g2d_calls=%lu g2d_fail=%lu g2d_avg=%.3f g2d_max=%.3f\n",
           m_chn,
           m_bypassCopyProbe ? 1 : 0,
           m_useG2dCopy ? 1 : 0,
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
           syncMaxMs,
           m_perfG2dCalls,
           m_perfG2dFails,
           g2dAvgMs,
           g2dMaxMs);

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
    m_decodeTimingActive = false;
    m_decodeTimingCallbackUs = 0;
    const char* bypassEnv = getenv("RTSPPULL_BYPASS_VDEC_COPY");
    m_bypassCopyProbe = (bypassEnv != nullptr && bypassEnv[0] != '\0' && strcmp(bypassEnv, "0") != 0);
    m_bypassCopyFrameCount = 0;
    m_perfG2dCalls = 0;
    m_perfG2dFails = 0;
    m_perfG2dUsTotal = 0;
    m_perfG2dUsMax = 0;
    if (m_bypassCopyProbe)
    {
        printf("[vdec-probe] chn=%d bypass copy/Ion enabled via RTSPPULL_BYPASS_VDEC_COPY\n", m_chn);
    }
}

int t507_vdec_node::sendFrame(media_frame *frame)
{
    int ret;
    AVPacket pkt;
    struct timespec time;
    const unsigned char CODE_SPS[5] = {0x00, 0x00, 0x00, 0x01, 0x67};
    int newW, newH;

    if (!m_bCreated)
    {
        logError("vdec[%d] have not been created. \n", m_chn);
        usleep(50 * 1000);
        return -1;
    }

    if (frame == NULL)
    {
        logError("frame is null\n");
        return -1;
    }

    if (frame->getPayloadType() != MEDIA_PT_H264)
    {
        logError("VDEC(%d) payload type must be MEDIA_PT_H264\n", m_chn);
        return -1;
    }

    memset(&pkt, 0, sizeof(AVPacket));
    clock_gettime(CLOCK_MONOTONIC, &time);
    frame->lockPacket(0, (void **)&pkt.pAddrVir0, NULL);
    pkt.dataLen0 = frame->getPacketSize(0);
    pkt.id = (++m_count);
    pkt.pts = (long long)time.tv_sec * 1000000 + time.tv_nsec / 1000;

    FrameType ft = parseH264FrameType(pkt.pAddrVir0, pkt.dataLen0);
    // logDebug("addrVir0 = %#x,  nalLen=%d,  m_decoder=%#x\n", pkt.pAddrVir0, pkt.dataLen0, m_decoder);
    // logDebug("get %s frame\n",getFrameTypeName(ft));


    if (memcmp(pkt.pAddrVir0, CODE_SPS, 5) == 0)
    {
        h264_decode_sps(pkt.pAddrVir0 + 5, pkt.dataLen0, &newW, &newH);
        if ((m_width != newW) || (m_height != newH))
        {
            logWarn("oldW=%d, oldH=%d, newW=%d, newH=%d\n", m_width, m_height, newW, newH);
            printf("\n\n\n###### new vdec attr!!! #######\n\n\n");
            m_width = newW;
            m_height = newH;
            destroy();
            create();
            return 0;
        }
    }

    {
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        m_decodeTimingActive = true;
        m_decodeTimingCallbackUs = 0;
    }
    const uint64_t decodeStartUs = monotonicTimeUs();
    ret = m_decoder->decode(&pkt);
    const uint64_t decodeEndUs = monotonicTimeUs();
    const uint64_t decodeElapsedUs = decodeEndUs - decodeStartUs;
    {
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        const uint64_t nestedCallbackUs = m_decodeTimingCallbackUs;
        const uint64_t decodeOnlyUs = decodeElapsedUs >= nestedCallbackUs ? (decodeElapsedUs - nestedCallbackUs) : 0;
        m_decodeTimingActive = false;
        m_decodeTimingCallbackUs = 0;
        ++m_perfDecodeCalls;
        accumulatePerfDuration(decodeElapsedUs, m_perfDecodeUsTotal, m_perfDecodeUsMax);
        accumulatePerfDuration(nestedCallbackUs, m_perfDecodeNestedCallbackUsTotal, m_perfDecodeNestedCallbackUsMax);
        accumulatePerfDuration(decodeOnlyUs, m_perfDecodeOnlyUsTotal, m_perfDecodeOnlyUsMax);
        if (ret < 0)
        {
            ++m_perfDecodeFails;
        }
        maybeLogPerfWindowLocked(decodeEndUs);
    }
    if (ret < 0)
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

        size_t scLen = 0;
        unsigned nalType = 0;
        if (findStartCode(pkt.pAddrVir0, pkt.dataLen0, &scLen) && pkt.dataLen0 > scLen)
        {
            nalType = pkt.pAddrVir0[scLen] & 0x1F;
        }

        if (m_decodeFailStreak == 1 || (m_decodeFailStreak % 20) == 0)
        {
            printf("[vdec-fail] chn=%d ret=%d streak=%u total=%lu nal=%u frameType=%s inputLen=%u retained=%zu retainedBytes=%zu head=%02X %02X %02X %02X\n",
                   m_chn,
                   ret,
                   m_decodeFailStreak,
                   m_decodeFailCount,
                   nalType,
                   getFrameTypeName(ft),
                   pkt.dataLen0,
                   retainedCount,
                   retainedBytes,
                   pkt.dataLen0 > 0 ? pkt.pAddrVir0[0] : 0,
                   pkt.dataLen0 > 1 ? pkt.pAddrVir0[1] : 0,
                   pkt.dataLen0 > 2 ? pkt.pAddrVir0[2] : 0,
                   pkt.dataLen0 > 3 ? pkt.pAddrVir0[3] : 0);
        }
        return -1;
    }

    if (m_decodeFailStreak > 0)
    {
        printf("[vdec-recover] chn=%d streak=%u total=%lu frameType=%s inputLen=%u\n",
               m_chn,
               m_decodeFailStreak,
               m_decodeFailCount,
               getFrameTypeName(ft),
               pkt.dataLen0);
        m_decodeFailStreak = 0;
    }

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

media_frame *t507_vdec_node::getFrame()
{
    return m_frame[m_curFrame];
}

int t507_vdec_node::decoderDataReady(awvideodecoder::AVPacket *packet)
{
    int ret = 0;
    frame_shell frame;
    // ion_mem mem;
    static int bufIndex[MAX_CAM_NUM] = {0};
    const uint64_t cbStartUs = monotonicTimeUs();
    uint64_t copyElapsedUs = 0;
    uint64_t syncElapsedUs = 0;

    if (m_bypassCopyProbe)
    {
        ++m_bypassCopyFrameCount;
        const uint64_t cbEndUs = monotonicTimeUs();
        {
            std::lock_guard<std::mutex> perfLock(m_perfMutex);
            const uint64_t callbackElapsedUs = cbEndUs - cbStartUs;
            ++m_perfCallbackCalls;
            if (m_decodeTimingActive)
            {
                ++m_perfCallbackInsideDecodeCalls;
                m_decodeTimingCallbackUs += callbackElapsedUs;
            }
            else
            {
                ++m_perfCallbackOutsideDecodeCalls;
            }
            accumulatePerfDuration(callbackElapsedUs, m_perfCallbackUsTotal, m_perfCallbackUsMax);
            maybeLogPerfWindowLocked(cbEndUs);
        }
        if (m_bypassCopyFrameCount == 1 || (m_bypassCopyFrameCount % 60) == 0)
        {
            printf("[vdec-copy-bypass] chn=%d callbacks=%lu inputLen=%u pts=%lld\n",
                   m_chn,
                   m_bypassCopyFrameCount,
                   packet != nullptr ? packet->dataLen0 : 0,
                   packet != nullptr ? packet->pts : 0);
        }
        return 0;
    }

        my_buffer::getInstance()->getVideobuffer(m_chn, T507_PREVIEW_BUF_NUM + bufIndex[m_chn], &mem);

    bool usedG2d = false;
    if (m_useG2dCopy && m_g2dHandle >= 0 && ALIGN_16B(m_height) == 1088)
    {
        const uint64_t srcY = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(packet->pAddrPhy0));
        const uint64_t srcC = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(packet->pAddrPhy1));
        const int dstFd = mem.dmafd;
        const uint64_t g2dStartUs = monotonicTimeUs();
        const int g2dRet = g2dBlitNv21ToFd(m_g2dHandle,
                                           srcY,
                                           srcC,
                                           static_cast<__u32>(m_width),
                                           static_cast<__u32>(ALIGN_16B(m_height)),
                                           static_cast<__u32>(m_width),
                                           static_cast<__u32>(m_height),
                                           dstFd,
                                           static_cast<__u32>(IMAGEWIDTH),
                                           static_cast<__u32>(IMAGEHEIGHT),
                                           static_cast<__u32>(m_width),
                                           static_cast<__u32>(m_height));
        copyElapsedUs = monotonicTimeUs() - g2dStartUs;
        {
            std::lock_guard<std::mutex> perfLock(m_perfMutex);
            ++m_perfG2dCalls;
            accumulatePerfDuration(copyElapsedUs, m_perfG2dUsTotal, m_perfG2dUsMax);
            if (g2dRet != 0)
            {
                ++m_perfG2dFails;
            }
        }

        if (g2dRet == 0)
        {
            usedG2d = true;
            const uint64_t syncStartUs = monotonicTimeUs();
            IonDmaSync(mem.dmafd);
            syncElapsedUs = monotonicTimeUs() - syncStartUs;
        }
        else
        {
            const int savedErrno = errno;
            m_useG2dCopy = false;
            printf("[vdec-g2d] chn=%d ioctl BITBLT_H failed ret=%d errno=%d(%s) srcY=%#llx srcC=%#llx dstFd=%d src=%dx%d/%dx%d dst=%dx%d/%dx%d, fallback to memcpy\n",
                   m_chn,
                   g2dRet,
                   savedErrno,
                   strerror(savedErrno),
                   static_cast<unsigned long long>(srcY),
                   static_cast<unsigned long long>(srcC),
                   dstFd,
                   m_width,
                   ALIGN_16B(m_height),
                   m_width,
                   m_height,
                   IMAGEWIDTH,
                   IMAGEHEIGHT,
                   m_width,
                   m_height);
        }
    }

    if (!usedG2d)
    {
        if (ALIGN_16B(m_height) == 1088)
        {
            const uint64_t copyStartUs = monotonicTimeUs();
            memcpy((void *)mem.virt, packet->pAddrVir0, 1920 * 1080);
            memcpy((void *)mem.virt + 1920 * 1080, packet->pAddrVir0 + 1920 * 1088, 1920 * 1080 / 2);
            copyElapsedUs = monotonicTimeUs() - copyStartUs;
            const uint64_t syncStartUs = monotonicTimeUs();
            IonDmaSync(mem.dmafd);
            syncElapsedUs = monotonicTimeUs() - syncStartUs;
        }
        else if (ALIGN_16B(m_height) == 720)
        {
            const uint64_t copyStartUs = monotonicTimeUs();
            for (int i = 0; i < 720; i++)
                memcpy((void *)mem.virt + i * 1920, packet->pAddrVir0 + i * 1280, 1280);
            for (int i = 0; i < 720 / 2; i++)
                memcpy((void *)mem.virt + 1920 * 1080 + i * 1920, packet->pAddrVir0 + 1280 * 720 + i * 1280, 1280);
            copyElapsedUs = monotonicTimeUs() - copyStartUs;
            const uint64_t syncStartUs = monotonicTimeUs();
            IonDmaSync(mem.dmafd);
            syncElapsedUs = monotonicTimeUs() - syncStartUs;
        }
    }
    // 高一定要对齐 否则回放视频会有一道绿边

    // frame.refill(MEDIA_PT_YUV_420SP_NV21, (void *)mem.virt,mem.phy, m_width, m_height, packet->pts, false);
    m_frame[bufIndex[m_chn]]->refill(MEDIA_PT_YUV_420SP_NV21, (void *)mem.virt, mem.phy, m_width, m_height, packet->pts, false);

    m_curFrame = bufIndex[m_chn];
    // logDebug(" bufIndex[m_chn] = %d\n", bufIndex[m_chn]);

    bufIndex[m_chn]++;
    if (bufIndex[m_chn] == T507_PLAYBACK_BUF_NUM)
        bufIndex[m_chn] = 0;

    {
        std::lock_guard<std::mutex> lock(m_retainedInputsMutex);
        if (m_retainedInputs.size() > kRetainedInputSafetyCount)
        {
            // printf("======> vdec[%d] retained input buffers count: %zu, bytes: %zu, dropping oldest buffer\n", m_chn, m_retainedInputs.size(), m_retainedInputBytes);
            m_retainedInputBytes -= m_retainedInputs.front()->size();
            m_retainedInputs.pop_front();
        }
    }

    const uint64_t cbEndUs = monotonicTimeUs();
    {
        std::lock_guard<std::mutex> perfLock(m_perfMutex);
        const uint64_t callbackElapsedUs = cbEndUs - cbStartUs;
        ++m_perfCallbackCalls;
        if (m_decodeTimingActive)
        {
            ++m_perfCallbackInsideDecodeCalls;
            m_decodeTimingCallbackUs += callbackElapsedUs;
        }
        else
        {
            ++m_perfCallbackOutsideDecodeCalls;
        }
        accumulatePerfDuration(callbackElapsedUs, m_perfCallbackUsTotal, m_perfCallbackUsMax);
        accumulatePerfDuration(copyElapsedUs, m_perfCopyUsTotal, m_perfCopyUsMax);
        accumulatePerfDuration(syncElapsedUs, m_perfSyncUsTotal, m_perfSyncUsMax);
        maybeLogPerfWindowLocked(cbEndUs);
    }

    return 0;
}

