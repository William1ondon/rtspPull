#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <string>

#include "my_log.h"
// 读取264码流文件

#define DEFAULT_BUF_LEN   (4*1024*1024)

// H.264 NALU类型定义
enum NaluType {
    NALU_TYPE_UNSPECIFIED = 0,
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,        // IDR帧
    NALU_TYPE_SEI = 6,        // SEI信息
    NALU_TYPE_SPS = 7,        // SPS
    NALU_TYPE_PPS = 8,        // PPS
    NALU_TYPE_AUD = 9,        // AUD
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12
};

class my_264test
{
private:
    /* data */
    std::string filePath_;

    FILE* inFile_;

    int decodePos_;
    int fileSize_;

    char* inputBuf_ ;
    int intputLen_ ;
public:
    my_264test(std::string &file_path);
    ~my_264test();

    char *getFrame(int*);

    int updateFileSeek(int len);

private:
    int find264NALFragment(char *data, size_t size, int *fragSize);

    bool checkFileValidation(const char* filename);
    int openFile();
    int getFileSize();
};
