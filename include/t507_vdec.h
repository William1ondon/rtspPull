#pragma once

#include "AWVideoDecoder.h"
#include "my_buffer.h"
#include "my_log.h"
#include "media_frame.h"
#include "DmaIon.h"
#include <list>
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
};
