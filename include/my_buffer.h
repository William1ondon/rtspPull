#pragma once

#include "ION_DRM/ion.h"
#include "ION_DRM/drm_fourcc.h"
#include <vector>
#include "common.h"

#define T507_PREVIEW_BUF_NUM 10
#define T507_PLAYBACK_BUF_NUM 2

typedef struct
{
	unsigned long phy;    /* physical address */
	unsigned long virt;   /* virtual address */
	unsigned long size;   /* ion buffer size */
	int dmafd;            /* ion dmabuf fd */
}ion_mem;

class my_buffer
{
private:
    static my_buffer *singleInstance;
	std::vector<ion_mem> m_VideoBuf[DISP_CHN_NUM];
public:
    static my_buffer* getInstance();
	int getVideobuffer(int chn,int index,ion_mem* pMem);
	int AllocVideoBuffer(int chn,int size);
	int AllocIonBuffer(ion_mem *mem, int size);
private:
    my_buffer();
	~my_buffer();
};