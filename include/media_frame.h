/* 帧外壳：
*	数据信息全部由外部传入
*	frame_shell仅仅用于封装数据格式，不负责申请和释放空间
*/

#ifndef _MEDIA_FRAME_H_
#define _MEDIA_FRAME_H_

#include "media_define.h"

class frame_shell: public media_frame
{
private:
	int m_ref_cnt;
	MEDIA_PAYLOAD_TYPE m_pt;
	void *m_virAddr;
	unsigned long m_phyAddr;
	unsigned long long m_pts;
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_pktNum;
	unsigned int m_size;
	bool m_bKeyFlag;

public:
	frame_shell();
	~frame_shell();
	bool refill(MEDIA_PAYLOAD_TYPE pt, void *virAddr, unsigned long phyAddr, unsigned int width, unsigned int height, unsigned long long pts, bool bKeyFlag);
	MEDIA_PAYLOAD_TYPE getPayloadType();
	unsigned long long getPts();
	bool isKeyFlag();
	unsigned int getWidth();
	unsigned int getHeight();
	unsigned int getSize();
	unsigned int getPacketNum();
	unsigned int getPacketSize(unsigned int index);
	int lockPacket(unsigned int index, void** virAddr, unsigned long *phyAddr);
	int unLockPacket(unsigned int index);
	void *getPrivateData();	  // 无效
	int release();				//无效
	void increase_cnt();		//无效
};

#endif /* _MEDIA_FRAME_H_ */

