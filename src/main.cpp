#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <string.h>
#include <signal.h>

#include "my_opengl.h"
#include "common.h"

#include "t507_vdec.h"
#include "my_264test.h"
#include "live555.h"

#define FILE_TEST

bool bThreadShouldStop = false;

t507_vdec_node *g_vdecNode = nullptr;
my_264test *g_264File = nullptr;
FILE *g_out = nullptr;

static void *runOpenGL(void *arg)
{
    int fd = -1;
    CT507Graphics::getInstance()->initOpenGL();

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(3, &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

    while (!bThreadShouldStop)
    {
        CT507Graphics::getInstance()->picDraw();
    }
}

bool startOpengl()
{
    pthread_t ret = 0;
    pthread_attr_t attr;

    // 单独设置一个线程用于创建和刷新openGL
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&ret, &attr, runOpenGL, nullptr);
    return true;
}

static int vi_chn = 0;

static void *listen_body(void *arg)
{

    int chn_num = vi_chn; // 当前的线程所表示的vi通道

    int ret, fd, maxFd = 0;

    struct timeval tv;
    fd_set read_fds;
    frame_shell *pFrameDec = nullptr;

    pFrameDec = new frame_shell;
    long long startTime = 0;
    long long endTime = 0;

    if (chn_num < DISP_CHN_NUM)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(1 + chn_num, &mask);
        pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    }

    printf("listen_body:chn:%d\n", chn_num);

    usleep(2500 * 1000);

    while (!bThreadShouldStop)
    {
        startTime = sv_safeFunc_GetTimeTick();
        logInfo("start time = %ld\n", startTime);
        int nalLen = 0;
        char *temp = g_264File->getFrame(&nalLen);
        g_264File->updateFileSeek(nalLen);

        if (temp == nullptr)
        {
            bThreadShouldStop = true;
            break;
        }

        pFrameDec->refill(MEDIA_PT_H264, temp, 0, nalLen, 1, 0, false);
        g_vdecNode->sendFrame(pFrameDec);

        media_frame *tempFrame = g_vdecNode->getFrame();
        void *virAddr;
        tempFrame->lockPacket(0, &virAddr, NULL);

        CT507Graphics::getInstance()->load_texture(virAddr, 1920, 1080, chn_num);
        endTime = sv_safeFunc_GetTimeTick();
        logInfo("end time = %ld\n", endTime);
        logWarn("all time = %ld\n", endTime - startTime); // 16ms

        if (endTime - startTime < 1000 / FRAME_RATE)
        {
            usleep((endTime - startTime) * 1000);
        }
    }
}

bool startChn()
{
    pthread_t ret = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* 开启视频预览和编码 */
    for (int i = 0; i < DISP_CHN_NUM; i++)
    {
        printf("ready to start chn: i = %d\n", i);
        vi_chn = i;
        pthread_create(&ret, &attr, listen_body, nullptr);
        usleep(200 * 1000);
    }
    pthread_attr_destroy(&attr);
    return true;
}

static void exit_handle(int signalnum)
{
    bThreadShouldStop = true;
}

static void recvSignal(int sig)
{

    bThreadShouldStop = true;
}

// usage : v4l2_to_screen ([0-CAM_MAX])
int main(int argc, char *argv[])
{
    signal(SIGINT, exit_handle);
    signal(SIGKILL, exit_handle);
    signal(SIGSEGV, recvSignal);

    rtspPuller puller("rtsp://192.168.88.88/mainstream");
    puller.loop();
    
#ifdef FILE_TEST
    // std::string h264Path = "/mnt/tmp.h264";
    std::string h264Path = "/mnt/1080P30.h264";
    // std::string h264Path = "/mnt/rtp_dump.h264"; // error
    g_264File = new my_264test(h264Path);
#endif

    g_vdecNode = new t507_vdec_node(0);

    // alloc buffer
    for (int i = 0; i < DISP_CHN_NUM; i++)
    {
        my_buffer::getInstance()->AllocVideoBuffer(i, 1920 * 1088 * 3 / 2);
    }

    // init openGL
    CT507Graphics::getInstance();

    // start vdec
    if (g_vdecNode->create() == -1)
    {
        return 1;
    }

    // create thread to run OpenGL
    startOpengl();

    // create thread to Q buffer
#ifdef FILE_TEST
    startChn();
#endif

    while (!bThreadShouldStop)
    {
        usleep(1000 * 1000);
    }
    return 1;
}