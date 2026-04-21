#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "sunxiMemInterface.h"
#include "my_buffer.h"
#include "my_log.h"

my_buffer* my_buffer::singleInstance = NULL;
static dma_mem_des_t g_cdxMemCtx = {};

my_buffer::my_buffer()
{
    memset(&g_cdxMemCtx, 0, sizeof(g_cdxMemCtx));
    allocOpen(MEM_TYPE_CDX_NEW, &g_cdxMemCtx, NULL);
}

my_buffer::~my_buffer()
{
    if(g_cdxMemCtx.ops != NULL)
    {
        allocClose(MEM_TYPE_CDX_NEW, &g_cdxMemCtx, NULL);
        g_cdxMemCtx.ops = NULL;
    }
}

my_buffer* my_buffer::getInstance()
{
    if(singleInstance == NULL)
        singleInstance = new my_buffer;
    return singleInstance;
}

int my_buffer::getVideobuffer(int chn, int index, ion_mem* pMem)
{
    if(index >= 0 && m_VideoBuf[chn].size() > static_cast<size_t>(index))
    {
        *pMem = m_VideoBuf[chn][index];
        return 0;
    }
    else
    {
        return -1;
    }
}

int my_buffer::AllocVideoBuffer(int chn, int size)
{
    ion_mem mem;

    for(int i = 0; i < T507_PREVIEW_BUF_NUM + T507_PLAYBACK_BUF_NUM; i++)
    {
        dma_mem_des_t req = {};
        req.ops = g_cdxMemCtx.ops;
        req.size = size;

        const int ret = allocAlloc(MEM_TYPE_CDX_NEW, &req, NULL);
        if(ret < 0 || req.vir == 0)
        {
            return -1;
        }

        mem.virt = req.vir;
        mem.size = size;
        mem.phy = req.phy;
        mem.dmafd = req.ion_buffer.fd_data.aw_fd;
        if(mem.dmafd < 0)
        {
            return -1;
        }
        m_VideoBuf[chn].push_back(mem);
    }
    return 0;
}

int my_buffer::AllocIonBuffer(ion_mem *mem, int size)
{
    dma_mem_des_t req = {};
    req.ops = g_cdxMemCtx.ops;
    req.size = size;

    const int ret = allocAlloc(MEM_TYPE_CDX_NEW, &req, NULL);
    if(ret < 0 || req.vir == 0)
    {
        return -1;
    }

    mem->virt = req.vir;
    mem->size = size;
    mem->phy = req.phy;
    mem->dmafd = req.ion_buffer.fd_data.aw_fd;
    if(mem->dmafd < 0)
    {
        return -1;
    }
    return mem->dmafd;
}

