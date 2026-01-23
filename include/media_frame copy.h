#pragma once

/* 媒体载荷数据类型 */
typedef enum
{
	/* 视频相关 */
	MEDIA_PT_H264,			//H264统称，可以是任何NAL
	MEDIA_PT_H264_HEADER,	//包括SPS和PPS，可能还有SEI
	MEDIA_PT_H264_I,
	MEDIA_PT_H264_P,
	MEDIA_PT_H265,			//H265统称，可以是任何NAL
	MEDIA_PT_H265_HEADER,	//包括SPS和PPS，可能还有SEI
	MEDIA_PT_H265_I,
	MEDIA_PT_H265_P,

	/* 音频相关 */
	MEDIA_PT_G711A,
	MEDIA_PT_G726,
	MEDIA_PT_ADPCM,
	MEDIA_PT_AAC,
	MEDIA_PT_LPCM,

	/* 图像相关 */
	MEDIA_PT_JPEG,
	MEDIA_PT_YUV_420SP_NV12,
	MEDIA_PT_YUV_420SP_NV21,
	MEDIA_PT_RGB_U8C1,
	MEDIA_PT_RGB_U16C1,
	MEDIA_PT_RGB_U8C3_PACKAGE,
	MEDIA_PT_RGB_U8C3_PLANAR,	

	/* 配置信息相关 */
	MEDIA_PT_KEY_CONFIG,

	MEDIA_PT_BUTT
}MEDIA_PAYLOAD_TYPE;

class media_frame
{
public:
	virtual ~media_frame() {}
	
	// 获取帧载荷类型
	virtual MEDIA_PAYLOAD_TYPE getPayloadType() = 0;
	// 获取时间戳
	virtual unsigned long long getPts() = 0;
	// 检查是否是关键帧
	virtual bool isKeyFlag() = 0;
	// 获取帧宽
	virtual unsigned int getWidth() = 0;
	// 获取帧高
	virtual unsigned int getHeight() = 0;
	// 获取帧大小
	virtual unsigned int getSize() = 0;
	// 获取数据包数量（一帧数据由N个数据包组成）
	virtual unsigned int getPacketNum() = 0;
	// 获取数据包大小（包序列为seq）
	virtual unsigned int getPacketSize(unsigned int seq) = 0;
	/* 直接获取数据包的虚拟地址和物理地址（便于减少数据拷贝的次数，要小心使用！）
	* 	当phyAddr为NULL，表示不获取物理地址
	* 	返回值：0成功，-1失败
	*/
	virtual int lockPacket(unsigned int seq, void** virAddr, unsigned long *phyAddr=nullptr) = 0;
	/* 解锁数据包，与lockPacket成对调用
	*	返回值：0成功，-1失败
	*/
	virtual int unLockPacket(unsigned int seq) = 0;
	// 获取帧私有数据，无私有数据则返回NULL
	virtual void *getPrivateData() = 0;
	// 释放帧数据。返回值：0成功，-1失败
	virtual int release() = 0;
	// 引用计数加1
	virtual void increase_cnt() = 0;
};



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
