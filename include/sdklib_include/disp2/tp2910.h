#ifndef TP2910_H
#define TP2910_H

#include "typedef.h"

typedef struct _tp2910_register
{
    u8 reg_addr;
    u8 value;
} tp2910_register;

/* 设备数量和设备名称 */
#define TP2910_CNT 1
#define TP2910_NAME "tp2910"

#define TP2910_IOC_MAGIC        '1'
#define TP2910_READ_REG         _IOWR(TP2910_IOC_MAGIC, 0, tp2910_register)
#define TP2910_WRITE_REG        _IOWR(TP2910_IOC_MAGIC, 1, tp2910_register)

// dac contrl
#define TP2910_POWER_DOWN_MODE  _IO(TP2910_IOC_MAGIC, 3)
#define TP2910_STANDBY_MODE     _IO(TP2910_IOC_MAGIC, 4)
#define TP2910_NORMAL_MODE      _IO(TP2910_IOC_MAGIC, 5)

// reg Definitions
#define TP2910_DAC_CTRL_REG 0x45

#define FAILURE -1

#endif