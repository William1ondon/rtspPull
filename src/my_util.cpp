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
        file.write(reinterpret_cast<const char *>(address), size); // 将地址的缓存内容写入文件
        file.close();
        printf("************* saved *************\n");
    }
    else
    {
        printf("************* can not open file *************\n");
    }
}
