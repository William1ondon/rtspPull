#ifndef _DEFINES_H_
#define _DEFINES_H_

#if __cplusplus
extern "C"{
#endif /* __cplusplus */

#define PATH_LEN 256
#define CMD_LEN  512


/* 数据类型定义 */
typedef signed char         sint8;
typedef signed short        sint16;
typedef signed int          sint32;
typedef signed long         slng32;
typedef signed long long    sint64;
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long       ulng32;
// typedef unsigned long long  uint64;

typedef unsigned char           sv_u8;
typedef unsigned short          sv_u16;
typedef unsigned int            sv_u32;
typedef signed char             sv_s8;
typedef short                   sv_s16;
typedef int                     sv_s32;
typedef unsigned long long      sv_u64;
typedef long long               sv_s64;
typedef char                    sv_char;
typedef char*                   sv_pchar;
typedef float                   sv_float;
typedef double                  sv_double;
typedef void                    sv_void;
typedef unsigned long           sv_size_t;


typedef unsigned char           SV_U8;
typedef unsigned short          SV_U16;
typedef unsigned int            SV_U32;
typedef signed char             SV_S8;
typedef short                   SV_S16;
typedef int                     SV_S32;
typedef unsigned long long      SV_U64;
typedef long long               SV_S64;
typedef char                    SV_CHAR;
typedef char*                   SV_PCHAR;
typedef float                   SV_FLOAT;
typedef double                  SV_DOUBLE;
typedef void                    SV_VOID;
typedef unsigned long           SV_SIZE_T;


typedef unsigned char           uint8_t;
typedef unsigned short          uint16_t;
typedef unsigned int            uint32_t;
typedef signed char             sint8_t;
typedef short                   sint16_t;
typedef int                     sint32_t;


#define sv_null          0L
#define sv_success       0
#define sv_failure       (-1) 

#ifndef NULL
#define NULL             0L
#endif

#define SV_SUCCESS          0
#define SV_FAILURE          (-1)

#define SV_DVR_LICENSE_DISABLE_P		".:?&|*/%_<>"

typedef enum
{
    sv_false = 0,
    sv_true  = 1
}
sv_bool;


typedef enum
{
    SV_FALSE= 0, 
    SV_TRUE = 1
} SV_BOOL;
	


/* 坐标信息 */
typedef struct tagPoint_S
{
    sint32 s32X;
    sint32 s32Y;
} SV_POINT_S;

/* 大小信息 */
typedef struct tagSize_S
{
    uint32 u32Width;
    uint32 u32Height;
} SV_SIZE_S;

/* 矩形区域 */
typedef struct tagRect_S
{
    sint32 s32X;
    sint32 s32Y;
    uint32 u32Width;
    uint32 u32Height;
} SV_RECT_S;

typedef enum
{
    THREAD_IS_EXIT = 0x00,  /* 线程退出 */
    THREAD_IS_RUNNING,      /* 线程运行 */
    THREAD_IS_STOP,         /* 线程停止 */
} THREAD_STATUS;

/* 模块软件版本定义 */
typedef struct
{
    uint32 minver : 6;  /* 0~63 */
    uint32 secver : 6;  /* 0~63 */
    uint32 majver : 5;  /* 0~31 */
    uint32 day : 5;     /* 日: 1~31 */
    uint32 month : 4;   /* 月: 1~12 */
    uint32 year : 6;    /* 年: 0~63 */
} SWVERSION, *PSWVERSION;
#define SWVERSION_LEN   sizeof(SWVERSION)

/* 算术运算定义 */
#define UMIN(a, b)      ((a) < (b) ? (a) : (b)) /* a <= b */
#define IMIN(a, b)      UMIN(a, b)
#define IMAX(a, b)      ((a) > (b) ? (a) : (b)) /* a > b */

#define SV_NULL          0L
#define SV_NULL_PTR      0L
#define SV_SUCCESS       0
#define SV_FAILURE       (-1)


#if __cplusplus
}
#endif /* __cplusplus */


#endif /* _DEFINES_H_ */
