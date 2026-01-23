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

my_buffer::my_buffer()
{
	IonAllocOpen();
}

my_buffer::~my_buffer(){}

my_buffer* my_buffer::getInstance()
{
	if(singleInstance == NULL)
		singleInstance = new my_buffer;
	return singleInstance;
}

int my_buffer::getVideobuffer(int chn, int index, ion_mem* pMem)
{
	if(m_VideoBuf[chn].size() > index)
	{
		*pMem = m_VideoBuf[chn][index];
		return 0;
	}
	else
	{
		printf("out of range!\n");
		return -1;
	}
}

int my_buffer::AllocVideoBuffer(int chn, int size)
{
	ion_mem mem;

	for(int i = 0;i < T507_PREVIEW_BUF_NUM + T507_PLAYBACK_BUF_NUM;i ++)
	{
		mem.virt = IonAlloc(size);
		mem.size = size;
		mem.phy = IonVir2phy((void *)mem.virt);
		mem.dmafd = IonVir2fd((void *)mem.virt);
		if(mem.dmafd < 0)
		{
			printf("IonAlloc failed!\n");
			return -1;
		}
		m_VideoBuf[chn].push_back(mem);

        logDebug("buffer[%d] = %ld\n",i, mem.virt);
    }
	printf("m_VideoBuf[%d].size() = %d\n",chn,m_VideoBuf[chn].size());
	return 0;
}


int my_buffer::AllocIonBuffer(ion_mem *mem, int size)
{
	mem->virt = IonAlloc(size);
	mem->size = size;
	mem->phy = IonVir2phy((void *)mem->virt);
	mem->dmafd = IonVir2fd((void *)mem->virt);
	if(mem->dmafd < 0)
	{
		printf("IonAlloc failed!\n");
		return -1;
	}
	return mem->dmafd;
}