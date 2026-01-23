#ifndef _MEDIA_DEFINE_H_
#define _MEDIA_DEFINE_H_

#include <stdio.h>
#include "media_osd.h"
#include <list>
#include "media_type.h"
//#include "graphics_type.h"

#if defined(QZ_HD291_WHYDM01)
#define TRIGGER_NUM   8
#else
#define TRIGGER_NUM   5    // 触发路数
#endif


/* 媒体节点类型 */
typedef enum
{
	MEDIA_NODE_VI = 0,
	MEDIA_NODE_VO,
	MEDIA_NODE_VPSS,
	MEDIA_NODE_VENC,
	MEDIA_NODE_VDEC,
	MEDIA_NODE_AI,
	MEDIA_NODE_AO,
	MEDIA_NODE_AENC,
	MEDIA_NODE_ADEC,
	MEDIA_NODE_FILE,
	MEDIA_NODE_POOL,
	MEDIA_NODE_IPC,
	MEDIA_NODE_DETECT,

	
	MEDIA_NODE_BUTT
}MEDIA_NODE_TYPE;

/* 媒体节点名称 */
static const char *const node_name[] = 
{
    "VI",
    "VO",
    "VPSS",
    "VENC",
    "VDEC",
    "AI",
    "AO",
    "AENC",
    "ADEC",
    "FILE",
    "POOL",
    "IPC",
    "DETECT"
};

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

/* 分辨率 */
typedef enum
{
	MEDIA_RESOLUTION_QCIF = 0,
	MEDIA_RESOLUTION_CIF,
	MEDIA_RESOLUTION_D1,
	MEDIA_RESOLUTION_720P,
	MEDIA_RESOLUTION_1080P,
	
	MEDIA_RESOLUTION_BUTT
}MEDIA_RESOLUTION_E;

/* 编码码率控制模式 */
typedef enum
{	
	MEDIA_RC_MODE_VBR,
	MEDIA_RC_MODE_CBR,
	MEDIA_RC_MODE_ABR,
	MEDIA_RC_MODE_FIXQP,
	
	MEDIA_RC_MODE_BUTT
}MEDIA_RC_MODE_E;

/* 音频采样率 */
typedef enum
{
	AUDIO_SAMP_RATE_8K  = 0, 	// 8kHz 采样率
	AUDIO_SAMP_RATE_16K, 		// 16kHz 采样率
	AUDIO_SAMP_RATE_32K, 		// 32kHz 采样率
	
	AUDIO_SAMP_RATE_BUTT,
} AUDIO_SAMP_RATE_E;

/* 音频采样精度 */
typedef enum
{
    AUDIO_WIDTH_8BIT   = 0,   	// 8bit 宽度
    AUDIO_WIDTH_16BIT  = 1,   	// 16bit 宽度
    
    AUDIO_WIDTH_BUTT,
} AUDIO_WIDTH_E;

/* 音频声道模式 */
typedef enum
{
	AUDIO_SOUND_MONO   = 0, 	// 单声道
	AUDIO_SOUND_STEREO = 1, 	// 双声道
	
	AUDIO_SOUND_BUTT
} AUDIO_SOUND_E;

/* 屏幕分割类型 */
typedef enum
{
    SCREEN_SPLIT_1,         	// 单通道显示 
    SCREEN_SPLIT_2,         	// 二分屏
    SCREEN_SPLIT_3,        	 	// 三分屏
    SCREEN_SPLIT_4_1,         	// 四分屏 — 模式1
    SCREEN_SPLIT_4_2,         	// 四分屏 — 模式2
    SCREEN_SPLIT_8,         	// 八分屏
    SCREEN_SPLIT_9,         	// 九分屏
    SCREEN_SPLIT_16,       		// 十六分屏
    
    VO_SPLIT_BUTT
} SCREEN_SPLIT_E;

/* 输出设备类型 */
typedef enum
{  
	SCREEN_TYPE_CVBS_N = 0,		// CVBS设备 (NTSC) 
	SCREEN_TYPE_CVBS_P,			// CVBS设备 (PAL) 
	SCREEN_TYPE_HDMI,			// HDMI设备 
	SCREEN_TYPE_VGA,			// CVBS设备
	SCREEN_TYPE_LCD,			// LCD设备
	SCREEN_TYPE_VIT,			// 虚拟设备 

    SCRREN_TYPE_BUTT
} SCREEN_TYPE_E;

typedef enum
{
	FULL_SCREEN_MODE1 = 0,
	FULL_SCREEN_MODE2,
	FULL_SCREEN_MODE3,
	FULL_SCREEN_MODE4,
	NORMAL_2_SPLIT_MODE,
	LEFT_3_SPLIT_MODE,
	//RIGHT_3_SPLIT_MODE,
	TOP_3_SPLIT_MODE,
	BOTTOM_3_SPLIT_MODE,
	H_4_SPLIT_MODE,
	NORMAL_4_SPLIT_MODE,
	PIP1,
	PIP2,
	PIP3,
	PIP4,

#if defined(QZ_HD291_HQHL03_1) || defined(QZ_HD291_HQHL03_2) || defined(QZ_HD140_WMYRB17)
	EXTEND_SPLIT_MODE,
#endif
	
	SCREEN_MODE_BOTTOM
}SPLIT_MODE;
typedef enum
{
    P_FULL_SCREEN_MODE1 = 0,
    P_FULL_SCREEN_MODE2,
    P_FULL_SCREEN_MODE3,
    P_FULL_SCREEN_MODE4,
    P_NORMAL_2_SPLIT_MODE,
    P_LEFT_3_SPLIT_MODE,
    //RIGHT_3_SPLIT_MODE,
    P_TOP_3_SPLIT_MODE,
    P_BOTTOM_3_SPLIT_MODE,
    P_H_4_SPLIT_MODE,
    P_NORMAL_4_SPLIT_MODE,
    // PIP1,
    // PIP2,
    // PIP3,
    P_PIP4,

    P_SCREEN_MODE_BOTTOM
}P_SPLIT_MODE;
typedef struct
{
	int coord_x;
	int coord_y;
	int width;
	int height;
}SPLIT_PIP;

typedef struct SPLIT_CONFIG
{
	SPLIT_MODE split_mode;  // 分割模式
	int        split_chn[MEDIA_CHANNEL_CNT];
	SPLIT_PIP  pip_params[4];  // 只有在pip画面下此参数才有效 表示4个PIP画面的信息值
	bool       posEnable[4];   // 四个位置哪个不显示
	
	// 等号运算符重载
	SPLIT_CONFIG &operator=(const SPLIT_CONFIG& tmp) 
	{
		// 如果是对象本身, 则直接返回
		if(this == &tmp)
		{
			return *this;
		}
		
		this->split_mode = tmp.split_mode;
		for(int i=0; i<MEDIA_CHANNEL_CNT; i++)
		{
			split_chn[i] = tmp.split_chn[i];
		}
		return *this;
	}
}__attribute__((packed))SPLIT_CONFIG; // 分割配置参数

typedef struct RTSP_SPLIT_CONFIG
{
    SPLIT_MODE split_mode;
    int        split_chn[MEDIA_CHANNEL_CNT];
    SPLIT_PIP  pip_params[4];
    bool       posEnable[4];

    RTSP_SPLIT_CONFIG &operator=(const RTSP_SPLIT_CONFIG& tmp)
    {
        if(this == &tmp)
        {
            return *this;
        }

        this->split_mode = tmp.split_mode;
        for(int i = 0; i < MEDIA_CHANNEL_CNT; i++)
        {
            split_chn[i] = tmp.split_chn[i];
        }
        return *this;
    }
}__attribute__((packed))RTSP_SPLIT_CONFIG;

typedef struct
{
	SPLIT_MODE split_mode;  // 当前用户选定的分割模式
#if (defined(QZ_HD291_PRE)) || (defined(QZ_HD291_123456)) || defined(QZ_HD277_000000)
    int audio_mix_enable;
    int audio_mix_output_chn;
    int audio_mix_val;
#endif
	unsigned int pos_chn[MEDIA_CHANNEL_CNT];      // 任何一种分割模式下 4个位置分别对应的chn
	unsigned int posEnable[MEDIA_CHANNEL_CNT];  // 每一个位置下对应的视频是否enable 注意此参数只与位置对应而不对应视频通道
	unsigned int audioOut;  // 每一种分割模式下使用的是哪一路音频输出
	SPLIT_MODE autoScanSerial[7];    // 自动扫描顺序
	unsigned short autoScanDelay[7]; // 自动扫描时间
	bool autoScanEnalble;  // 自动扫码是否开启
	//int pipInfo[12][4];  // 3中pip模式下共12幅分割画面的信息
	int pipInfo[16][4];  // 4中pip模式下共16幅分割画面的信息
}__attribute__((packed))ALL_SPLIT_CONFIG;

typedef struct
{
	SPLIT_MODE split_mode;
	unsigned int pos_chn[MEDIA_CHANNEL_CNT];
	unsigned int posEnable[MEDIA_CHANNEL_CNT];
	int pipInfo[16][4];
}__attribute__((packed))ALL_RTSP_SPLIT_CONFIG;

typedef struct
{
	SPLIT_MODE split_mode;
#if (defined(QZ_HD291_PRE)) || (defined(QZ_HD291_123456)) || defined(QZ_HD277_000000)
    int audio_mix_enable;
    int audio_mix_output_chn;
    int audio_mix_val;
#endif
	unsigned char pos_chn[SCREEN_MODE_BOTTOM][MEDIA_CHANNEL_CNT];
	bool posEnable[SCREEN_MODE_BOTTOM][MEDIA_CHANNEL_CNT];
	unsigned char audioOut[SCREEN_MODE_BOTTOM];
	SPLIT_MODE autoScanSerial[7];    // 自动扫描顺序
	unsigned short autoScanDelay[7]; // 自动扫描时间
	bool autoScanEnalble;
	int pip1Info[4][4];  // pip信息
	int pip2Info[4][4];
	int pip3Info[4][4];
	int pip4Info[4][4];
}__attribute__((packed))SPLITCONFIG_4GUI;

typedef struct
{
    SPLIT_MODE split_mode;
    unsigned char pos_chn[SCREEN_MODE_BOTTOM][MEDIA_CHANNEL_CNT];
    bool posEnable[SCREEN_MODE_BOTTOM][MEDIA_CHANNEL_CNT];
    int pip1Info[4][4];  // pip信息
    int pip2Info[4][4];
    int pip3Info[4][4];
    int pip4Info[4][4];
}__attribute__((packed))RTSP_SPLITCONFIG_4GUI;

enum _E_DETECT_SENSITIVITY
{
    E_DETECT_SENSITIVITY_OFF = 0,
    E_DETECT_SENSITIVITY_LOW,
    E_DETECT_SENSITIVITY_HIGH,

    E_DETECT_SENSITIVITY_BUTT
};

typedef struct 
{
   int sensitivity; // 0 : close; 1: low; 2: high
   // 冻结检测参数 deprecated
//    int Delta_Y;
//    int Delta_N;
//    int Delta_F;

//    int blurTop;    // 模糊阈值
//    int blurBototm; // 模糊阈值

}__attribute__((packed))DETECT_PARAMS;

/* VI属性 */
typedef struct
{
	unsigned int width;  // 前端AD获取的图像的宽和高
	unsigned int height;
	bool bMirror;			// 镜像
	bool bFlip;				// 翻转
    int rotate;             // 旋转
}__attribute__((packed))MEDIA_VI_ATTR;

typedef struct
{
	//unsigned int width;  // 前端AD获取的图像的宽和高
	//unsigned int height;
	bool bMirror;			// 镜像
	bool bFlip;				// 翻转
    int rotate;    // 0:无旋转    1:左转90°   2:右转90° 3:旋转180°
	int brightness;
	int contrast;
	int saturation;
	int hue;
	#if defined(QZ_HD291_PRE) || defined(QZ_HMU318)|| (defined(QZ_HMU318_200008)) || (defined(QZ_HD291_123456)) || defined(QZ_HD277_000000)
	char chn_name[16];
	unsigned char autoPower;
	unsigned char audioVal;
	#endif
}__attribute__((packed))CAMERA_PARAMS; // camera界面参数配置

typedef struct
{
	FORMAT_TYPE_E        sysType[MEDIA_CHANNEL_CNT];
	FORMAT_RESOLUTION_E  sysResolution[MEDIA_CHANNEL_CNT];
	bool                 bAutomatic;
}__attribute__((packed))STANDARD_CONFIG;

// gui通知我媒体是否打开或关闭VI
typedef struct
{
	bool bStart;  // = false表示关闭, =true表示开启
	char chnNum;  // 掩码表示需要改变的通道
	STANDARD_CONFIG standardConf;
}__attribute__((packed))STANDARD_INFO;

/* VPSS属性 */
typedef struct
{
	unsigned int width;
	unsigned int height;
	bool bIeEn;				// 图像增强
	bool bDciEn;			// 动态对比度调节
	bool bNrEn;				// 去噪
}MEDIA_VPSS_ATTR;

/* VO属性 */
/*typedef struct
{
	SCREEN_SPLIT_E split;	// 屏幕分割模式
	unsigned int chnMask;	// 显示通道掩码
}MEDIA_VO_ATRR;*/

typedef struct
{
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
}MEDIA_RECT;

typedef struct
{
	bool Mirror[MEDIA_CHANNEL_CNT];
	bool Flip[MEDIA_CHANNEL_CNT];
    int  Rotate[MEDIA_CHANNEL_CNT];
	SPLIT_CONFIG splitMode;     // 分割模式
	RTSP_SPLIT_CONFIG rtspSplitMode; //rtsp分割模式
}MEDIA_VO_ATRR;

// 输入信号源参数设置
typedef struct
{
    int inputSrcMode;  // 为0表示无VGA/HDMI输入
    int signalSource;  // signalSource: 0--camera 1--vga 2--HDMI
}__attribute__((packed)) INPUT_SRC_PARAM;

/* AI/AO属性 */
typedef struct
{
    AUDIO_SAMP_RATE_E 	sampRate;   	// 音频采样率 (ACC编码协议只支持16K) 
    AUDIO_WIDTH_E 		bitWidth; 		// 音频采样精度
    AUDIO_SOUND_E		soundMode;
}MEDIA_AIO_ATTR;

/* AENC属性 */
typedef struct
{
	MEDIA_PAYLOAD_TYPE pt;
}MEDIA_AENC_ATTR;


typedef struct tagVencJpeg_S
{
    unsigned int	priority;   /* 通道优先级(暂时无效) */
    //unsigned int  	width;    	/* 编码图片宽度 */
    //unsigned int   	height;   	/* 编码图片高度 */
    unsigned int   	quality;  	/* 抓拍图片质量, [0-100] 数值越大质量越好 */
} VIDEO_ENCODE_JPEG_S;

typedef struct
{
   // unsigned int	width;    	/* 编码图像宽度 [H264E_MIN_WIDTH,H264E_MAX_WIDTH] */
    //unsigned int	height;   	/* 编码图像高度 [H264E_MIN_HEIGHT,H264E_MAX_HEIGHT] */
    unsigned int  	srcFrmRate;	/* 输入帧率 [1, 60] fps */
    unsigned int  	dstFrmRate;	/* 目标帧率 [1, srcFrmRate] fps ，当dstFrmRate和srcFrmRate相等时表示不进行帧率控制*/
    MEDIA_RC_MODE_E	rcMode;   	/* 码率控制模式 */
    unsigned int	bitrate;   	/* 编码码率大小 [1,20000]Kpbs : CBR/ABR-表示平均码率, VBR-表示最大码率 */
    int	minQp;       /* 最小Qp值(VBR模式有效) [0,50]数值越小质量越好(-1设置为默认值) */
    int	maxQp;       /* 最大Qp值(VBR模式有效) [0,50]数值越小质量越好(-1设置为默认值) */
    int	IQp;         /* I帧Qp值(FIXQP模式有效)[0,50]数值越小质量越好(-1设置为默认值) */
    int	PQp;         /* I帧Qp值(FIXQP模式有效)[0,50]数值越小质量越好(-1设置为默认值) */
    //VideoRoiParam_S roiParamArray[ROI_MAX];		/**/
} VIDEO_ENCODE_H264_S;

typedef struct
{
    //unsigned int	width;    	/* 编码图像宽度 [H264E_MIN_WIDTH,H264E_MAX_WIDTH] */
    //unsigned int	height;   	/* 编码图像高度 [H264E_MIN_HEIGHT,H264E_MAX_HEIGHT] */
    unsigned int  	srcFrmRate;	/* 输入帧率 [1, 60] fps */
    unsigned int  	dstFrmRate;	/* 目标帧率 [1, srcFrmRate] fps ，当dstFrmRate和srcFrmRate相等时表示不进行帧率控制*/
    MEDIA_RC_MODE_E	rcMode;   	/* 码率控制模式 */
    unsigned int	bitrate;   	/* 编码码率大小 [1,20000]Kpbs : CBR/ABR-表示平均码率, VBR-表示最大码率 */
    int	minQp;       /* 最小Qp值(VBR模式有效) [0,50]数值越小质量越好(-1设置为默认值) */
    int	maxQp;       /* 最大Qp值(VBR模式有效) [0,50]数值越小质量越好(-1设置为默认值) */
    int	IQp;         /* I帧Qp值(FIXQP模式有效)[0,50]数值越小质量越好(-1设置为默认值) */
    int	PQp;         /* I帧Qp值(FIXQP模式有效)[0,50]数值越小质量越好(-1设置为默认值) */
    //VideoRoiParam_S roiParamArray[ROI_MAX];		/**/
} VIDEO_ENCODE_H265_S;

//typedef int (*MEDIA_DATA_CALLBACK)(unsigned int chn, MEDIA_PAYLOAD_TYPE type, unsigned long long pts, unsigned char *pdata, unsigned int size);
typedef int(*MEDIA_DATA_CALLBACK)(unsigned int s32Ch, unsigned char *u8StrBuf, unsigned int size, unsigned long long pts, unsigned char bKey);
typedef int(*MEDIA_DATA_HEADER_CALLBACK)(unsigned int s32Ch, unsigned char *pData, unsigned int length);

/* VENC属性
* 编码数据有两种输出方式（互斥）：回调和绑定
* 	当callback为不NULL，采用回调函数
*	当callback为NULL，并且venc.output使能，则数据自动投递到下一级被绑定的NODE
*/	
typedef struct
{
	MEDIA_PAYLOAD_TYPE pt;		// 载荷数据类型(MEDIA_PT_H264，MEDIA_PT_H265，MEDIA_PT_JPEG)
	unsigned int	srcWidth;	// 前端原始图像宽高
	unsigned int	srcHeight;
	unsigned int	dstWidth;	// 后端编码后图像宽高
	unsigned int	dstHeight;
	int osdConf;				// 0 去使能 1 使能原像 2 使能镜像
	MEDIA_DATA_CALLBACK callback;	// 写编码数据到ringbuffer
	MEDIA_DATA_HEADER_CALLBACK headerCallback;	// 将sps和pps头信息更新到ringbuffer
	char plateNum[128]; // 车牌号
	unsigned int  fileDur; // 录像时长: 单位为秒
	union
	{
		VIDEO_ENCODE_H264_S H264Attr;
		VIDEO_ENCODE_H265_S H265Attr;
		VIDEO_ENCODE_JPEG_S JpegAttr;
	};
}MEDIA_VENC_ATTR;


/* VDEC属性
* 解码数据有两种输出方式（互斥）：回调和绑定
* 	当callback为不NULL，采用回调函数
*	当callback为NULL，并且vdec.output使能，则数据自动投递到下一级被绑定的NODE
*/
typedef struct
{
    unsigned int Width;
    unsigned int Height;
    MEDIA_DATA_CALLBACK callback; 	// 解码数据回调函数
}MEDIA_VDEC_ATTR;

// 解码传入的数据类型
typedef struct {
    char            chFileName[MEDIA_CHANNEL_CNT][255];   /* 文件名: 包含文件路径的完整文件名 */
	unsigned char   u8ChMask;             /* 通道掩码: 表示播放几路文件 */
    //SV_S32            s32FileType;
    //SV_S32            s32PBAudio;
} __attribute__((packed)) MEDIA_PB_FILE_INFO_S;



class media_input;
class media_output;
class media_node;

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
	virtual int lockPacket(unsigned int seq, void** virAddr, unsigned long *phyAddr=NULL) = 0;
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



/* 负责媒体系统初始化和内存管理 */
class media_sys
{
public:
	virtual ~media_sys() {}

	// 媒体系统初始化
	virtual int init() = 0;
	// 媒体系统去初始化
	virtual int fini() = 0;

	virtual int malloc(unsigned long *phyAddr, void **virAddr, unsigned int size, const char* memName, bool bCache) = 0;	
	virtual int free(unsigned long phyAddr, void *virAddr) = 0;
	// 获取SOC温度
	virtual int getTemp(double *temp) = 0;
	virtual int getHdmiStat() = 0;
	virtual int getHdmiEDID() = 0;
//	virtual int setConfig() = 0;
};

class media_input
{
public:
	virtual ~media_input() {}	
	// 获取所属节点
	virtual media_node *getNode() = 0;
	// 获取输入口标识ID
	virtual int getInputId() = 0;
	// 向输入口发送帧数据
	virtual int sendFrame(media_frame *frame) = 0;
	// 检查输入口是否被绑定
	virtual bool isBound() = 0;
	// 获取输入口被绑定到的对象
	virtual media_output *getBindObject() = 0;
	// 解绑
	virtual bool unbind() = 0;
	// 设置输入口帧率( -1表示不进行帧率控制)
	virtual int setFrameRate(int dstFrameRate, int srcFrameRate) {return -1;}
	/*区域附着（目前只支持：OSD）
	*	handle: 区域索引
	*	rel_x: 相对坐标X，范围0 - 1000
	*	rel_y: 相对坐标Y，范围0 - 1000
	*	media_osd: 要附着的OSD对象
	*	返回值：0成功，-1失败
	*/
	virtual int attach(unsigned int handle, int rel_x, int rel_y, media_osd *osd) {return -1;}
	// 去附着。返回值：0成功，-1失败
	virtual int detach(unsigned int handle) {return -1;}
};

class media_output
{
public:
	virtual ~media_output() {}
	// 获取所属节点
	virtual media_node *getNode() = 0;
	// 获取输出口标识ID
	virtual int getOutputId() = 0;
	// 查询是否使能输出口数据流出
	virtual bool isEnable() = 0;
	// 使能输出口数据流出。返回值：0成功，-1失败
	virtual int enable() = 0;
	// 禁止输出口数据流出。返回值：0成功，-1失败
	virtual int disable() = 0;
	// 获取输出口绑定次数
	virtual int getBindCnt() = 0; 
	// 获取被绑定到输出口的input集，-1表示失败， >=0表示被绑定的Input数量
	virtual int getBoundObject(media_input **inputArray) = 0;
	// 将output绑定到input
	virtual bool bind(media_input *input) = 0;
	// 将output与input解绑
	virtual bool unbind(media_input *input) = 0;
	// 从输出口获取一帧数据（输出口只负责生产不负责释放， 帧数据有自主释放的能力）
	virtual media_frame *getFrame() = 0;
	// 获取输出图像大小。返回值：0成功，-1失败
	virtual int getPicSize(unsigned int *width, unsigned int *height) {return -1;}
	// 设置输出图像大小。返回值：0成功，-1失败
	virtual int setPicSize(unsigned int width, unsigned int height) {return -1;}
	// 获取句柄。返回值：0成功，-1失败
	virtual int getFd() {return -1;}
};

class media_node
{
public:
	virtual ~media_node() {}

	// 节点是否被创建
	virtual bool isCreated() = 0;
	// 获取节点类型
	virtual MEDIA_NODE_TYPE getNodeType() = 0;
	// 获取节点名
	const char *getNodeName() {return node_name[getNodeType()];}
	// 创建节点。返回值：0成功，-1失败
	virtual int create() = 0;
	// 销毁节点。返回值：0成功，-1失败
	virtual int destroy() = 0;
};

class media_vi_node: public media_node
{
public:
	virtual ~media_vi_node() {}

	// 设置VI属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_VI_ATTR *attr) = 0;
	// 获取VI属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_VI_ATTR *attr) = 0;
	virtual int setMirrorAndFlip(bool bMirror, bool bFlip) = 0;
	virtual int getMirrorAndFlip(bool *bMirror, bool *bFlip) = 0;
	// 获取输出口。返回值：0成功，-1失败
	virtual media_output *getOutput() = 0;
};

class media_vpss_node: public media_node
{
public:
	virtual ~media_vpss_node() {}
	
	// 设置VPSS属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_VPSS_ATTR *attr) = 0;
	// 获取VPSS属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_VPSS_ATTR *attr) = 0;
	// 获取输入口
	virtual media_input *getInput() = 0;
	// 获取输出口
	virtual media_output *getOutput(int chn) = 0;
};

class media_venc_node: public media_node
{
public:
	virtual ~media_venc_node() {}

	// 设置VENC属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_VENC_ATTR *attr) = 0;
	// 获取VENC属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_VENC_ATTR *attr) = 0;	
	// 获取输入口
	virtual media_input *getInput() = 0;
	// 获取输出口
	virtual media_output *getOutput() = 0;

	virtual void frametoScale(unsigned char *src_data, int src_width, int src_height, unsigned char** dst_data, int dst_width, int dst_height) = 0;
};

class media_vo_node: public media_node
{
public:
	virtual ~media_vo_node() {}

	// 设置VO属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_VO_ATRR *pVoAttr) = 0;
	// 获取VO属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_VO_ATRR *pVoAttr) = 0;
	// 获取输入
	virtual media_input *getInput(int chn) = 0;
	virtual media_output *getOutput() = 0;
};

class media_ai_node: public media_node
{
public:
	virtual ~media_ai_node() {}

	// 设置AI属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_AIO_ATTR *attr) = 0;
	// 获取AI属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_AIO_ATTR *attr) = 0;
};

class media_aenc_node: public media_node
{
public:
	virtual ~media_aenc_node() {}

	// 设置AENC属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_AENC_ATTR *attr) = 0;
	// 获取AENC属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_AENC_ATTR *attr) = 0;
};

class media_ao_node: public media_node
{
public:
	virtual ~media_ao_node() {}

	// 设置AO属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_AIO_ATTR *attr) = 0;
	// 获取AO属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_AIO_ATTR *attr) = 0;
};

class media_vdec_node: public media_node
{
public:
	virtual ~media_vdec_node() {}

	// 设置VDEC属性。返回值：0成功，-1失败
	virtual int setAttr(const MEDIA_VDEC_ATTR *attr) = 0;
	// 获取VDEC属性。返回值：0成功，-1失败
	virtual int getAttr(MEDIA_VDEC_ATTR *attr) = 0;
	// 获取输入口
	virtual media_input *getInput() = 0;
	// 获取输出口
	virtual media_output *getOutput() = 0;
};

class media_detect_node: public media_node
{
public:
    virtual ~media_detect_node() {}

    // 设置VDEC属性。返回值：0成功，-1失败
    virtual int setAttr(const DETECT_PARAMS *attr) = 0;
    // 获取VDEC属性。返回值：0成功，-1失败
    virtual int getAttr(DETECT_PARAMS *attr) = 0;
    // 获取输入口
    virtual media_input *getInput() = 0;
    // 获取输出口
    virtual media_output *getOutput() = 0;
};


#endif /* _MEDIA_DEFINE_H_ */

