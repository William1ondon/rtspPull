#include "my_264test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

my_264test::my_264test(std::string &file_path)
{
    filePath_ = file_path;

    inFile_ = nullptr;
    inputBuf_ = nullptr;

    decodePos_ = 0;
    fileSize_ = 0;

    // check file
    checkFileValidation(filePath_.c_str());

    // open file
    openFile();

    // get file size
    getFileSize();


    inputBuf_ = (char*)malloc(DEFAULT_BUF_LEN);
    intputLen_ = DEFAULT_BUF_LEN;

}

my_264test::~my_264test()
{
}

bool my_264test::checkFileValidation(const char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        logError("Filename is NULL or empty\n");
        return false;
    }

    // 检查文件路径长度
    if (strlen(filename) > PATH_MAX)
    {
        logError("Filename too long (%zu > %d)\n", strlen(filename), PATH_MAX);
        return false;
    }

    // 检查文件是否存在
    if (access(filename, F_OK) != 0)
    {
        printf("File does not exist: %s\n", filename);
        return false;
    }

    // 检查是否是常规文件（不是目录、符号链接等）
    struct stat st;
    if (stat(filename, &st) != 0)
    {
        logError("Cannot stat file: %s (%s)\n", filename, strerror(errno));
        return false;
    }

    if (!S_ISREG(st.st_mode))
    {
        logError("Not a regular file: %s\n", filename);
        return false;
    }

    // 检查文件大小
    if (st.st_size == 0)
    {
        logError("File is empty: %s\n", filename);
        return false;
    }

    if (st.st_size > 10 * 1024 * 1024 * 1024LL)
    { // 10GB限制
        logError("File too large (%lld bytes)\n", (long long)st.st_size);
        return false;
    }

    // 检查读取权限
    if (access(filename, R_OK) != 0)
    {
        logError("No read permission: %s\n", filename);
        return false;
    }

    return true;
}

int my_264test::getFileSize()
{
    fseek(inFile_, 0L, SEEK_END);
    fileSize_ = ftell(inFile_);
    fseek(inFile_, 0L, SEEK_SET);


    // logInfo("fileSize is  %d success\n",fileSize_);

    return 1;
}

int my_264test::openFile()
{
    inFile_ = fopen(filePath_.c_str(), "rb");
    if(inFile_ == NULL)
    {
        logError("Open file error: %s \n", filePath_.c_str());
        return -1;
    }
    // logInfo("Openfil %s success\n",filePath_.c_str());
    return 1;
}

int my_264test::find264NALFragment(char *data, size_t size, int *fragSize)
{
    static const char kStartCode4[4] = {0x00, 0x00, 0x00, 0x01};
    static const char kStartCode3[3] = {0x00, 0x00, 0x01};

    auto is_start = [&](size_t off) {
        return (off + 4 <= size && memcmp(&data[off], kStartCode4, 4) == 0) ||
               (off + 3 <= size && memcmp(&data[off], kStartCode3, 3) == 0);
    };

    if (size < 4) return -1;

    if (!is_start(0)) return -2;

    size_t offset = 4;
    while (offset + 3 < size && !is_start(offset))
    {
        ++offset;
    }

    if (offset > size - 4)
    {
        *fragSize = (int)size;
        return -3; // 需要继续读
    }

    *fragSize = (int)offset;
    return (int)(data[4] & 0x1f);
}

int my_264test::updateFileSeek(int nalLen)
{
    decodePos_ += nalLen;
    fseek(inFile_, decodePos_, SEEK_SET); //go to the real decode position.
    logDebug("decode pos = %d\n", decodePos_);
}

// char* my_264test::getFrame(int* len)
// {
//     int readTmpLen = 0;
//     int nalLen = 0;
//     int ret = 0;

//     if(decodePos_ < fileSize_)
//     {
//         logDebug("getFrame process[%d/%d]\n", decodePos_,fileSize_);

//         readTmpLen = ((fileSize_ - decodePos_) < intputLen_) ? (fileSize_ - decodePos_) : intputLen_;
//         fread(inputBuf_, 1, readTmpLen, inFile_);
//         nalLen = 0;
//         ret = find264NALFragment((char*)inputBuf_, readTmpLen, &nalLen);
//         if (nalLen < 4)
//         {
//             logError("find264NALFragment fail \n");
//             return nullptr;
//         }
//     }
//     else
//     {
//         logWarn("read finish!!\n");
//         return nullptr;
//     }

//     *len = nalLen;
//     return inputBuf_;
// }

char* my_264test::getFrame(int* len)
{
    if (decodePos_ >= fileSize_)
    {
        logError("read finish!!\n");
        return nullptr;
    }

    int nalLen = 0;
    int readTmpLen = ((fileSize_ - decodePos_) < intputLen_) ? (fileSize_ - decodePos_) : intputLen_;

    fread(inputBuf_, 1, readTmpLen, inFile_);

    while (true)
    {
        int ret = find264NALFragment(inputBuf_, readTmpLen, &nalLen);
        if (ret == -3)
        {
            int more = ((fileSize_ - decodePos_ - readTmpLen) < intputLen_) ? (fileSize_ - decodePos_ - readTmpLen) : intputLen_;
            if (more <= 0) { break; }

            inputBuf_ = (char*)realloc(inputBuf_, readTmpLen + more);
            fread(inputBuf_ + readTmpLen, 1, more, inFile_);
            readTmpLen += more;
            continue;
        }
        if (nalLen < 4)
        {
            logError("find264NALFragment fail \n");
            return nullptr;
        }
        break;
    }

    *len = nalLen;
    return inputBuf_;
}