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

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

using namespace awvideodecoder;


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
        // system("reboot -f");//不知道原因，只能重启
        return -1;
    }
    else
        logInfo("decoder init success. \n");

    m_bCreated = true;
    return 0;
}

int t507_vdec_node::destroy()
{
    if (m_decoder != NULL)
    {
        m_bCreated = false;
        usleep(100 * 1000); // 延时是让送过去的解码视频帧解完
        AWVideoDecoder::destroy(m_decoder);

        m_decoder = NULL;
    }
    else
        logWarn("vdec had been destoryed already. \n");

    return 0;
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
    logDebug("get %s frame\n",getFrameTypeName(ft));


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
    if (m_height == 1080)
        usleep(4 * 1000);
    if (m_height == 720)
        usleep(12 * 1000);
    ret = m_decoder->decode(&pkt);
    if (ret < 0)
    {
        logError("decode failed. [chn:%d, err:%d]\n", m_chn, ret);
        return -1;
    }

    return 0;
}

media_frame *t507_vdec_node::getFrame()
{
    return m_frame[m_curFrame];
}

// 1920x1080解码后得到的YUV实际大小1920x1088
int t507_vdec_node::decoderDataReady(awvideodecoder::AVPacket *packet)
{
    int ret = 0;
    frame_shell frame;
    // ion_mem mem;
    static int bufIndex[2] = {0};

    my_buffer::getInstance()->getVideobuffer(m_chn, T507_PREVIEW_BUF_NUM + bufIndex[m_chn], &mem);

    // logWarn("chn%d before memcpy,bufIndex num = %d, mem.virt = %ld\n",m_chn,bufIndex[m_chn],mem.virt);

    if (ALIGN_16B(m_height) == 1088)
    {
        memcpy((void *)mem.virt, packet->pAddrVir0, 1920 * 1080);                                 // Y
        memcpy((void *)mem.virt + 1920 * 1080, packet->pAddrVir0 + 1920 * 1088, 1920 * 1080 / 2); // Y
        IonDmaSync(mem.dmafd);
    }
    else if (ALIGN_16B(m_height) == 720)
    {
        for (int i = 0; i < 720; i++)
            memcpy((void *)mem.virt + i * 1920, packet->pAddrVir0 + i * 1280, 1280); // Y
        for (int i = 0; i < 720 / 2; i++)
            memcpy((void *)mem.virt + 1920 * 1080 + i * 1920, packet->pAddrVir0 + 1280 * 720 + i * 1280, 1280); // Y
        IonDmaSync(mem.dmafd);                                                                                  // 增加720p的ion同步，否则会出现动态画面撕裂
    }
    // 高一定要对齐 否则回放视频会有一道绿边

    // frame.refill(MEDIA_PT_YUV_420SP_NV21, (void *)mem.virt,mem.phy, m_width, m_height, packet->pts, false);
    m_frame[bufIndex[m_chn]]->refill(MEDIA_PT_YUV_420SP_NV21, (void *)mem.virt, mem.phy, m_width, m_height, packet->pts, false);

    m_curFrame = bufIndex[m_chn];
    // logDebug(" bufIndex[m_chn] = %d\n", bufIndex[m_chn]);

    bufIndex[m_chn]++;
    if (bufIndex[m_chn] == T507_PLAYBACK_BUF_NUM)
        bufIndex[m_chn] = 0;

    return 0;
}
