#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <error.h>
#include <errno.h>
#include <sys/ioctl.h>
//#define PRINT_LEVEL DEBUG
#include "media_frame.h"


frame_shell::frame_shell() 
{

}

frame_shell::~frame_shell() 
{

}

bool frame_shell::refill(MEDIA_PAYLOAD_TYPE pt, void *virAddr, unsigned long phyAddr, unsigned int width, unsigned int height, unsigned long long pts, bool bKeyFlag)
{
	m_pt = pt;
	m_virAddr = virAddr;
	m_phyAddr = phyAddr;
	m_width = width;
	m_height = height;
	m_pts = pts;
	m_bKeyFlag = bKeyFlag;
	switch(m_pt)
	{
		case MEDIA_PT_RGB_U8C3_PACKAGE:
		case MEDIA_PT_RGB_U8C3_PLANAR:
			m_pktNum = 3;
			m_size = m_width*m_height*3;
			break;
		case MEDIA_PT_YUV_420SP_NV12:
		case MEDIA_PT_YUV_420SP_NV21:
			m_pktNum = 2;
			m_size = m_width*m_height*3/2;
			break;	
		default:
			m_pktNum = 1;
			m_size = m_width*m_height;
	}

	return true;
}

MEDIA_PAYLOAD_TYPE frame_shell::getPayloadType()
{
	return m_pt;
}

unsigned long long frame_shell::getPts() 
{
	return m_pts;
}

bool frame_shell::isKeyFlag()	
{
	return m_bKeyFlag;
}

unsigned int frame_shell::getWidth() 
{
	return m_width;
}

unsigned int frame_shell::getHeight() 
{
	return m_height;
}

unsigned int frame_shell::getSize()
{
	return m_size;
}

unsigned int frame_shell::getPacketNum()
{
	return m_pktNum;
}

unsigned int frame_shell::getPacketSize(unsigned int index)
{
	return (m_width*m_height >> index);
}

int frame_shell::lockPacket(unsigned int index, void** virAddr, unsigned long *phyAddr)
{
	if(virAddr != NULL)
		*virAddr = (char*)m_virAddr + m_width*m_height*index;
	if(phyAddr != NULL)
		*phyAddr = m_phyAddr + m_width*m_height*index;
	
	return 0;
}

int frame_shell::unLockPacket(unsigned int index)
{
	return 0;
}

void *frame_shell::getPrivateData()
{
	return NULL;
}

int frame_shell::release()
{
	return 0;
}

void frame_shell::increase_cnt() 
{
	
}

