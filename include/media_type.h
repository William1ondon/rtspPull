#ifndef _MEDIA_TYPE_H_
#define _MEDIA_TYPE_H_

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef QZ_HD250
#define SV_MEDIA_CHANNEL_CNT        2
#define MEDIA_CHANNEL_CNT           2
#define FFMPEG_INIT_CHANNEL_CNT     2
#elif (defined(QZ_HD291_REC)) || (defined(QZ_HD291_PRE)) || (defined(QZ_HMU318))|| (defined(QZ_HMU318_200008)) || (defined(QZ_HD291_123456)) || defined(QZ_HD277_000000)
#define SV_MEDIA_CHANNEL_CNT        4
#define MEDIA_CHANNEL_CNT           4
#define FFMPEG_INIT_CHANNEL_CNT     4
#endif


/* 这个枚举值在avcodec_common.h中有定义，在这里又赋值一份的原因是: media_muxer.h包含了
 * 全志的AWVideoEncoder.h头文件 而这个鬼文件中定义了一个AVPacket类型 这个类型正好跟ffmpeg
 * 的头文件中定义的AVPacket重名，导致我如果在media_muxer.cpp中也包含avcodec_common.h就会
 * 导致AVPacket这个数据类型定义模糊 因为我media_muxer.cpp中只用到了下面的这个枚举值，因此
 * 我将其复制一份放在这里包含到media_muxer.cpp中，而media_muxer.cpp就不用再包含avcodec_common.h
 * 了 可以解开冲突
 */
enum CodecID {
    CODEC_ID_NONE,

    /* video codecs */
    CODEC_ID_RAWVIDEO,
	CODEC_ID_CINEPAK,
    CODEC_ID_H263,
    CODEC_ID_MPEG4,
    CODEC_ID_MSV1,
    CODEC_ID_H264,
    CODEC_ID_FFH264,
    CODEC_ID_AMV,

    /* various PCM "codecs" */
    CODEC_ID_PCM_S16LE = 0x10000,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_U16LE,
    CODEC_ID_PCM_U16BE,
    CODEC_ID_PCM_S8,
    CODEC_ID_PCM_U8,
    CODEC_ID_PCM_MULAW,
    CODEC_ID_PCM_ALAW,
    CODEC_ID_PCM_S32LE,
    CODEC_ID_PCM_S32BE,
    CODEC_ID_PCM_U32LE,
    CODEC_ID_PCM_U32BE,
    CODEC_ID_PCM_S24LE,
    CODEC_ID_PCM_S24BE,
    CODEC_ID_PCM_U24LE,
    CODEC_ID_PCM_U24BE,
    CODEC_ID_PCM_S24DAUD,
    CODEC_ID_PCM_ZORK,
    CODEC_ID_PCM_S16LE_PLANAR,
    CODEC_ID_PCM_DVD,
    CODEC_ID_PCM_F32BE,
    CODEC_ID_PCM_F32LE,
    CODEC_ID_PCM_F64BE,
    CODEC_ID_PCM_F64LE,
    CODEC_ID_PCM_BLURAY,
    /* various ADPCM codecs */
    CODEC_ID_ADPCM_IMA_QT = 0x11000,
    CODEC_ID_ADPCM_IMA_WAV,
    CODEC_ID_ADPCM_IMA_DK3,
    CODEC_ID_ADPCM_IMA_DK4,
    CODEC_ID_ADPCM_IMA_WS,
    CODEC_ID_ADPCM_IMA_SMJPEG,
    CODEC_ID_ADPCM_MS,
    CODEC_ID_ADPCM_4XM,
    CODEC_ID_ADPCM_XA,
    CODEC_ID_ADPCM_ADX,
    CODEC_ID_ADPCM_EA,
    CODEC_ID_ADPCM_G726,
    CODEC_ID_ADPCM_CT,
    CODEC_ID_ADPCM_SWF,
    CODEC_ID_ADPCM_YAMAHA,
    CODEC_ID_ADPCM_SBPRO_4,
    CODEC_ID_ADPCM_SBPRO_3,
    CODEC_ID_ADPCM_SBPRO_2,
    CODEC_ID_ADPCM_THP,
    CODEC_ID_ADPCM_IMA_AMV,
    CODEC_ID_ADPCM_EA_R1,
    CODEC_ID_ADPCM_EA_R3,
    CODEC_ID_ADPCM_EA_R2,
    CODEC_ID_ADPCM_IMA_EA_SEAD,
    CODEC_ID_ADPCM_IMA_EA_EACS,
    CODEC_ID_ADPCM_EA_XAS,
    CODEC_ID_ADPCM_EA_MAXIS_XA,
    CODEC_ID_ADPCM_IMA_ISS,
    /* AMR */
    CODEC_ID_AMR_NB = 0x12000,
    CODEC_ID_AMR_WB,

    /* various DPCM codecs */
    CODEC_ID_ROQ_DPCM = 0x14000,
    CODEC_ID_INTERPLAY_DPCM,
    CODEC_ID_XAN_DPCM,
    CODEC_ID_SOL_DPCM,

    /* audio codecs */
    CODEC_ID_MP2 = 0x15000,
    CODEC_ID_MP3,               ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
    CODEC_ID_AAC,

	/* subtitle codecs */
	CODEC_ID_DVD_SUBTITLE= 0x17000,
	CODEC_ID_DVB_SUBTITLE,
	CODEC_ID_TEXT,	///< raw UTF-8 text
	CODEC_ID_XSUB,
	CODEC_ID_SSA,
	CODEC_ID_MOV_TEXT,

};

#define STREAM_TYPE_CNT 			6
#define STREAM_TYPE_VEDIO_MAIN  	0
#define STREAM_TYPE_VEDIO_SUB 		1
#define STREAM_TYPE_AUDIO 			2
#define STREAM_TYPE_GPS 			3
#define STREAM_TYPE_GSENSOR 		4
#define STREAM_TYPE_AUDIO_SUB 		5

#define PLATE_NUMBER_STR_LEN_MAX 	128

#define SV_MEDIA_FILE_NAME_VERSION_PTR 14

#define SV_MEDIA_V02_FILE_LENGTH_MAX (124) // V02: MSV
#define SV_MEDIA_V02_FILE_LENGTH_MIN (122) // V02: AVI
#define SV_MEDIA_V01_FILE_LENGTH_MAX (113) // V01: MSV
#define SV_MEDIA_V01_FILE_LENGTH_MIN (112) // V01: AVI


typedef enum
{
	FILE_TYPE_AVI = 1,
	FILE_TYPE_H264,
	FILE_TYPE_MSV,
	FILE_TYPE_MP4,
	FILE_TYPE_CNT
}FILE_TYPE_E;


typedef struct
{
	SV_S32	s32Year;
	SV_S32	s32Month; 
	SV_S32	s32Day;
	SV_S32	s32Hour;
	SV_S32	s32Minute;
	SV_S32	s32Second;
	SV_CHAR cEventType[2];
	SV_S32	s32Duration;
	SV_S32	s32Size;
	SV_S32	s32Width;
	SV_S32	s32Height;
	SV_S32	s32FrameRate;
	SV_S32	s32BitRate;
	SV_S32	cPlateNum[32];
	SV_S32	s32ChNum;
	SV_U32	u32DeviceID;
	SV_U8	u8Flag;
	SV_S32	s32Msec;
	SV_S32	s32PreRecordMsec;
	SV_CHAR cVersionNum[32];
	SV_CHAR cCustomerNum[4];
	SV_CHAR cTimeZone[4];
	SV_U8 cDST;
	SV_CHAR cFileType[32];
}
SV_MEDIA_FILE_NAME_PARAMS_ST;


typedef struct
{
	/* 每一个元素代表这种数据类型的所有通道的录像掩码 */
	unsigned short	 u16ReqChMask[STREAM_TYPE_CNT];
	/* get stream chn mask: 获取到数据的标志掩码 同一类型下所有通道共用 */
	unsigned short	 u16GetStrChMask[STREAM_TYPE_CNT];
	/* 当前帧是否是I帧掩码: 同一数据类型下的所有通道共用*/
	unsigned short   u16KeyMask[STREAM_TYPE_CNT];
	/* 读取的这一帧数据的大小 */
	int			     s32Size[STREAM_TYPE_CNT][MEDIA_CHANNEL_CNT];
	/* 确定类型 确定通道下 指向从缓存区中拷贝出来的编码数据 实际内存地址是VIDEO_BUF */
	unsigned char 	 *pu8Addr[STREAM_TYPE_CNT][MEDIA_CHANNEL_CNT];
	int			     s32Width[STREAM_TYPE_CNT][MEDIA_CHANNEL_CNT];
	int			     s32Height[STREAM_TYPE_CNT][MEDIA_CHANNEL_CNT];
	/* 读取的这一帧数据的时间戳 */
	unsigned long long     u64Pts[STREAM_TYPE_CNT][MEDIA_CHANNEL_CNT];
}SV_MEDIA_VIDEO_WITH_AUDIO_PACKET_INFO_S;


typedef struct
{
	SV_U8 		  *pu8Addr;    /* 保存要写入的数据帧的地址 */
	SV_U8         u8Chn;       /* 当前是哪一通道 */
	SV_U8         u8Type;
	SV_BOOL		  bKeyFlag;
	SV_U32		  u32Size;     /* 当前要写入的数据帧的大小 */
	SV_U64        u64Pts;      /* 数据帧时间戳 */
	SV_U8		  u8PreTime;
	SV_BOOL		  bSetPreTime;

}MEDIA_WRITER_PACKET_INFO_S;

typedef enum
{
	PACKET_TYPE_VEDIO = 0,
	PACKET_TYPE_AUDIO,
	PACKET_TYPE_GPS,
	PACKET_TYPE_GFORCE,
	PACKET_TYPE_DATA,
	PACKET_TYPE_CNT
}PACKET_TYPE_E;


/* sps/pps头 信息结构体 */
#define UNIT_BUFFER_SIZE_VIDEO_MAIN	2000*1024
typedef struct
{
	char buf[1024];
	unsigned int size;
}HEADER_INFO;


/********* demuxer data type **********/

typedef enum
{
	RECORD_TYPE_NORMAL = 0,
	RECORD_TYPE_TIME,
	RECORD_TYPE_MOTION,
	RECORD_TYPE_SPEED,
	RECORD_TYPE_GSENSOR,
	RECORD_TYPE_ALARM1,     // = 5
	RECORD_TYPE_ALARM2,
	RECORD_TYPE_ALARM3,
	RECORD_TYPE_ALARM4,
	RECORD_TYPE_ALARM5,
	RECORD_TYPE_ALARM6,     // = 10
	RECORD_TYPE_ALARM7,
	RECORD_TYPE_ALARM8,
	RECORD_TYPE_ACCELERATION,
	RECORD_TYPE_DECELERATION,
	RECORD_TYPE_ACC_TURN,       // = 15
	RECORD_TYPE_GYR_TURN,
	RECORD_TYPE_IMPACT,
	RECORD_TYPE_GYR_TURN_LEFT,
	RECORD_TYPE_GYR_TURN_RIGHT,
	RECORD_TYPE_GYR_CLIP_FILE,    // = 20
   
	RECORD_TYPE_BUTT,
	RECORD_TYPE_CNT    // = 22
}MEDIA_RECORD_TYPE_E;


typedef struct
{
	SV_S32					 s32Ch;
	SV_S32					 s32Duration;
	SV_S32					 s32Size;
	SV_U64                   u64SizeByte; /* 文件大小: 一路文件的大小 */
	SV_S32					 stFileTime;  /* 文件创建时间: 以秒为单位 */
	SV_BOOL					 bRecording;
	MEDIA_RECORD_TYPE_E	     eRecordType; /* 录像文件类型 NM A1等*/
	FILE_TYPE_E				 fileType;

	SV_BOOL bRecordProtected;
	SV_BOOL bbRecordUploaded;
	SV_BOOL bbRecordExist;
	SV_BOOL bInvalidFlag;
	SV_BOOL bRepairedFlag;
}MEDIA_FILE_INFO_ST;

/* 定义回放界面操作命令枚举值 */
typedef enum
{
	OP_INIT = 0,        // 操作命令初始值 无实际功能
	OP_NEXT,            // 播放下一个
	OP_PREVIOUS,        // 播放上一个
	OP_SEEK_FORWARD,    // 向前跳转一次
	OP_SEEK_BACKWARD,   // 向后跳转一次
	OP_PAUSE,            // 暂停当前播放
	OP_EXIT,             // 退出播放界面 停止播放
	OP_DISPMODE          // 播放模式切换
}MEDIA_OP_E;

/* display界面设置操作命令枚举值 */
typedef enum
{
	OP_INITIAL = 0,      // 操作命令初始值 无实际功能
	OP_MIRROR,           // 设置镜像
	OP_FLIP,             // 设置翻转
    OP_ROTATE,           // 设置旋转
	OP_BRIGHTNESS,       // 设置亮度
	OP_CONTRAST,         // 设置对比度
	OP_SATURATION,       // 设置饱和度
	OP_HUE,              // 设置灰度 界面表示的是color
}DISPLAY_OP_E;

typedef struct
{
	DISPLAY_OP_E op_cmd;   // 操作命令
	int value;            // 命令对应的操作值
	int cameraIndex;      // 要设置的是哪一路: 0--Top 1---Bottom
}DISPLAY_PARAMS;

/* 定义在media_muxer.h中 存储最新的录像文件名 */
extern char file_name[MEDIA_CHANNEL_CNT][255];


/* 图像分辨率枚举类型(录像帧率也通过此枚举推算) */
typedef enum
{
	HD_1080p = 0,
	HD_720p,
	SD_960H,   /* 960*576 */
	SD_PAL,    /* 720*576 */
	SD_NSTC    /* 720*480 */
}MEDIA_PIC_SIZE_E;

/* 图像比特率枚举类型 */
typedef enum
{
	BITRATE_1M = 0, // low
	BITRATE_4M = 1, // middle
	BITRATE_8M = 2  // high
}MEDIA_BITRATE_E;

/* 录像时长枚举类型 */
typedef enum
{
	duration_5_min = 0,
	duration_10_min,
	duration_15_min
}RECORD_FILE_DURATION_E;


/* 此类型仅在媒体中单独使用 不再保存到参数模块中
 */
 typedef struct
{
	unsigned int			u32FrameRate;     /* 编码帧率 */
	unsigned int			bitRate;          /* 编码比特率 */
	MEDIA_PIC_SIZE_E		eResolution;      /* 编码分辨率 */
	RECORD_FILE_DURATION_E  fileDur;          /* 录像时长 */
	char 					plateNum[PLATE_NUMBER_STR_LEN_MAX];  /* 车牌号 */
}__attribute__((packed))MEDIA_VENC_PARAMS_S;


/* 录像参数结构体 */
typedef struct
{
	char 					bPowerOnRecord;     /* 是否允许开机录像 */
	char 					bCycleRecord;       /* 是否允许循环录像 */
	char 					bAlarmRecord;       /* 是否允许报警录像 */
	RECORD_FILE_DURATION_E  fileDur;           /* 循环录像时的文件时长 */
	FILE_TYPE_E 			fileType;          /* 录像文件类型: 暂不在界面开放 固定avi */
	int                     GSensorSensitivity; /* Gsensor灵敏度 */
	MEDIA_PIC_SIZE_E		eResolution;        /* 编码分辨率 */
	MEDIA_BITRATE_E			bitRate;            /* 编码比特率 */
	unsigned int			u32FrameRate;       /* 编码帧率 */
	unsigned int            u32FilterTime;       /*事件录像过滤时间*/
    unsigned int	        u32EventRecTime;      /*事件录像持续时间*/
    unsigned int            bEventRecLock;      /*事件录像锁*/
	char 					plateNum[PLATE_NUMBER_STR_LEN_MAX];  /* 车牌号 */
}__attribute__((packed))MEDIA_RECORD_PARAMS_S;

/*超速报警参数结构体*/
typedef struct
{
    unsigned int speedSource;   //速度源
    unsigned int speedUnit;     //速度单位
    unsigned int overSpeed;    //报警阈值
    unsigned int speed;        //当前速度
    unsigned int bAlarm;      //是否开启报警录像
    unsigned int duration;     //报警录像长度
	unsigned int gs_offorsensitivity;  //是否关闭gsensor
}__attribute__((packed))OVER_SPEED_PARAMS_S;

typedef struct
{
	unsigned char bMode;    // 三重态: 0--off     1--on   2--auto
	unsigned char type;     // 倒车光标类型 0--type1   2--type2
	int type1Coords[2][9];  // type1的9个顶点坐标
	int type2Coords[2][3];  // type2的3个顶点坐标

	double cal_type1_scale_x;
	double cal_type1_scale_y;
    double cal_type2_scale_x;
    double cal_type2_scale_y;
}__attribute__((packed))PARK_CONFIG;

typedef enum
{
    LANG_ENG = 0,   // 英语
    LANG_FRA,       // 法语
    LANG_ESP,       // 西班牙语
    LANG_NED,       // 荷兰语
    LANG_ITA,       // 意大利语
    LANG_DEU,       // 德语
    LANG_PYC,       // 俄语
    LANG_POR,       // 葡萄牙语
    LANG_POL,       // 波兰语
    LANG_TUR,       // 土耳其语
    LANG_JAP,       // 日语
    LANG_CHA,       // 中文
}__attribute__((packed))LANG_TYPE;

typedef enum
{
	TYPE_AHD = 0,
	TYPE_HDT,
	TYPE_PAL,
	TYPE_NTSC,
}FORMAT_TYPE_E;

typedef enum
{
	RESOLUTION_1080P30 = 0,
	RESOLUTION_1080P25,
	RESOLUTION_720P60,
	RESOLUTION_720P50,
	RESOLUTION_720P30,
	RESOLUTION_720P25,
}FORMAT_RESOLUTION_E;

typedef struct
{
    char           *pBuf;  /* 保存OBM中数据的首地址 */
    unsigned int    len;   /* 保存OBM中数据的长度 */
    long long       pts;   /* 保存OBM中数据的pts */
    int             bufId; /* 保存拿走数据的buf的id号 */
} AudioEncOutBuffer;

/* 消息回调函数错误码类型 */
typedef enum
{
	ERR_MEDIA_CLOSE_ALARM = 0,
	ERR_MEDIA_REC_REPEAT,
	ERR_MEDIA_UNKNOWN
}MEDIA_ERR_NO;

typedef struct
{
	int x;
	int y;
	int width;
	int height;
}PHY_FRAME;


#ifdef __cplusplus
}
#endif

#endif /* _MEDIA_TYPE_H_ */
