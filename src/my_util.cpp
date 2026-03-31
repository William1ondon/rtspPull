#include "my_util.h"

void save_yuv(const void *yuv_vAddr, char *file_path, int w, int h)
{
    FILE *fp;

    fp = fopen(file_path, "w+");
    if (fp == NULL)
    {
        printf("%s: Error: cannot open file !\n", __func__);
        return;
    }
    int ret = fwrite(yuv_vAddr, 1, w * h * 3 / 2, fp);
    if (ret <= 0)
    {
        printf("%s: Error: cannot write file !\n", __func__);
        return;
    }

    printf(" === save_yuv ====\n");

    fclose(fp);

    return;
}



void writeCacheToFile(const void *address, std::size_t size, const std::string &filename)
{
    std::ofstream file(filename, std::ios::binary | std::ios::app); // 以二进制模式打开文件

    if (file.is_open())
    {
        file.write(reinterpret_cast<const char *>(address), size); // 将地址的缓存内容写入文�?        file.close();
        printf("************* saved *************\n");
    }
    else
    {
        printf("************* can not open file *************\n");
    }
}

void MEDIA_MemcpyNeon (void *dest, const void *src, size_t len)
{
    uint8_t *dst = static_cast<uint8_t *>(dest);
    const uint8_t *srcBytes = static_cast<const uint8_t *>(src);
    const size_t len_16 = len / 16;

    for (size_t i = 0; i < len_16; ++i)
    {
        const uint8x16_t reg = vld1q_u8(srcBytes + i * 16);
        vst1q_u8(dst + i * 16, reg);
    }

    const size_t tail = len - len_16 * 16;
    if (tail > 0)
    {
        memcpy(dst + len_16 * 16, srcBytes + len_16 * 16, tail);
    }
}

void MEDIA_MemsetNeon (void *dest, int s, size_t len)
{
    uint8_t *dst = static_cast<uint8_t *>(dest);
    const uint8x16_t reg = vdupq_n_u8(static_cast<uint8_t>(s));
    const size_t len_16 = len / 16;

    for (size_t i = 0; i < len_16; ++i)
    {
        vst1q_u8(dst + i * 16, reg);
    }

    const size_t tail = len - len_16 * 16;
    if (tail > 0)
    {
        memset(dst + len_16 * 16, s, tail);
    }
}

