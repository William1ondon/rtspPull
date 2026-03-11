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
#include <pthread.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <thread>
#include <vector>

#include "my_opengl.h"
#include "common.h"

#include "my_264test.h"
#include "live555.h"
#include "my_util.h"

#define FILE_TEST

bool bThreadShouldStop = false;

my_264test* g_264File = nullptr;
FILE* g_out = nullptr;

struct DecodeThreadContext
{
    int chn;
    t507_vdec_node* vdecNode;
    H264Queue* h264Queue;
};

static std::array<t507_vdec_node*, DISP_CHN_NUM> g_vdecNodes{};
static std::array<H264Queue*, DISP_CHN_NUM> g_h264Queues{};
static std::array<rtspPuller*, DISP_CHN_NUM> g_pullers{};
static std::array<DecodeThreadContext, DISP_CHN_NUM> g_decodeCtx{};

static const uint8_t START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

static std::string trim(const std::string& str)
{
    size_t first = 0;
    while (first < str.size() && std::isspace(static_cast<unsigned char>(str[first])))
    {
        ++first;
    }

    size_t last = str.size();
    while (last > first && std::isspace(static_cast<unsigned char>(str[last - 1])))
    {
        --last;
    }

    return str.substr(first, last - first);
}

static std::vector<std::string> splitUrls(const std::string& raw)
{
    std::vector<std::string> urls;
    size_t start = 0;

    while (start <= raw.size())
    {
        size_t commaPos = raw.find(',', start);
        std::string token;

        if (commaPos == std::string::npos)
        {
            token = raw.substr(start);
        }
        else
        {
            token = raw.substr(start, commaPos - start);
        }

        token = trim(token);
        if (!token.empty())
        {
            urls.push_back(token);
        }

        if (commaPos == std::string::npos)
        {
            break;
        }
        start = commaPos + 1;
    }

    return urls;
}

static void* runOpenGL(void* arg)
{
    (void)arg;

    CT507Graphics::getInstance()->initOpenGL();

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(3, &mask);
    pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

    while (!bThreadShouldStop)
    {
        CT507Graphics::getInstance()->picDraw();
    }

    return nullptr;
}

bool startOpengl()
{
    pthread_t ret = 0;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&ret, &attr, runOpenGL, nullptr);
    pthread_attr_destroy(&attr);
    return true;
}

static void* listen_body(void* arg)
{
    DecodeThreadContext* ctx = static_cast<DecodeThreadContext*>(arg);
    if (ctx == nullptr || ctx->vdecNode == nullptr || ctx->h264Queue == nullptr)
    {
        logError("listen_body invalid context\n");
        return nullptr;
    }

    int chn_num = ctx->chn;

    if (chn_num < DISP_CHN_NUM)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(1 + chn_num, &mask);
        pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    }

    printf("listen_body: chn:%d\n", chn_num);

    H264Packet pkt;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    bool waitingForIDR = false;
    constexpr size_t kDecodeBufSlotCount = 64;
    std::array<std::vector<uint8_t>, kDecodeBufSlotCount> decodeSlots;
    size_t decodeSlotIdx = 0;

    while (!bThreadShouldStop)
    {
        if (!ctx->h264Queue->pop(pkt))
        {
            break;
        }

        if (pkt.data == nullptr || pkt.size == 0)
        {
            delete[] pkt.data;
            continue;
        }

        const uint8_t nalType = pkt.data[0] & 0x1F;

        if (waitingForIDR && nalType != 5 && nalType != 7 && nalType != 8)
        {
            delete[] pkt.data;
            continue;
        }

        if (nalType == 7)
        {
            sps.assign(pkt.data, pkt.data + pkt.size);
            delete[] pkt.data;
            continue;
        }

        if (nalType == 8)
        {
            pps.assign(pkt.data, pkt.data + pkt.size);
            delete[] pkt.data;
            continue;
        }

        size_t outSize = 4 + pkt.size;
        bool needExtra = false;

        if (pkt.isIDR && !sps.empty() && !pps.empty())
        {
            outSize += 4 + sps.size();
            outSize += 4 + pps.size();
            needExtra = true;
        }

        std::vector<uint8_t>& decodeBuf = decodeSlots[decodeSlotIdx];
        decodeSlotIdx = (decodeSlotIdx + 1) % kDecodeBufSlotCount;
        decodeBuf.resize(outSize);

        uint8_t* inputBuf = decodeBuf.data();
        uint8_t* p = inputBuf;

        if (needExtra)
        {
            memcpy(p, START_CODE, 4);
            p += 4;
            memcpy(p, sps.data(), sps.size());
            p += sps.size();

            memcpy(p, START_CODE, 4);
            p += 4;
            memcpy(p, pps.data(), pps.size());
            p += pps.size();
        }

        memcpy(p, START_CODE, 4);
        p += 4;
        memcpy(p, pkt.data, pkt.size);
        delete[] pkt.data;

        frame_shell fs;
        fs.refill(MEDIA_PT_H264, inputBuf, 0, outSize, 1, 0, pkt.isIDR);

        int sendRet = ctx->vdecNode->sendFrame(&fs);

        if (sendRet != 0)
        {
            waitingForIDR = true;
            continue;
        }

        if (pkt.isIDR)
        {
            waitingForIDR = false;
        }

        media_frame* tempFrame = ctx->vdecNode->getFrame();
        if (tempFrame == nullptr)
        {
            continue;
        }

        void* virAddr = nullptr;
        tempFrame->lockPacket(0, &virAddr, NULL);
        if (virAddr != nullptr)
        {
            CT507Graphics::getInstance()->load_texture(virAddr, IMAGEWIDTH, IMAGEHEIGHT, chn_num);
        }
    }

    return nullptr;
}

bool startChn()
{
    pthread_t ret = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (int i = 0; i < DISP_CHN_NUM; i++)
    {
        printf("ready to start decode channel: %d\n", i);
        pthread_create(&ret, &attr, listen_body, &g_decodeCtx[i]);
    }

    pthread_attr_destroy(&attr);
    return true;
}

static void exit_handle(int signalnum)
{
    (void)signalnum;
    bThreadShouldStop = true;
}

static void recvSignal(int sig)
{
    (void)sig;
    bThreadShouldStop = true;
}

static void print_usage(const char* prog)
{
    printf("Usage: %s [-u rtsp_url]... [-d queue_depth]\n", prog);
    printf("  -u : RTSP url. Repeat up to %d times, or pass comma-separated list\n", DISP_CHN_NUM);
    printf("       Example: -u rtsp://cam1/main -u rtsp://cam2/main -u rtsp://cam3/main -u rtsp://cam4/main\n");
    printf("  -d : H264 queue depth per channel (default: 8)\n");
}

int main(int argc, char* argv[])
{
    signal(SIGINT, exit_handle);
    signal(SIGKILL, exit_handle);
    signal(SIGSEGV, recvSignal);

#ifdef FILE_TEST
    // std::string h264Path = "/mnt/tmp.h264";
    // g_264File = new my_264test(h264Path);
#endif

    const std::string defaultUrl = "rtsp://192.168.88.146/mainstream";
    std::array<std::string, DISP_CHN_NUM> rtspUrls;
    rtspUrls.fill(defaultUrl);

    std::vector<std::string> userUrls;
    int queueDepth = 8;

    int opt;
    while ((opt = getopt(argc, argv, "u:d:h")) != -1)
    {
        switch (opt)
        {
            case 'u':
            {
                std::vector<std::string> parsed = splitUrls(optarg);
                if (parsed.empty())
                {
                    userUrls.emplace_back(optarg);
                }
                else
                {
                    userUrls.insert(userUrls.end(), parsed.begin(), parsed.end());
                }
                break;
            }

            case 'd':
                queueDepth = atoi(optarg);
                if (queueDepth <= 0)
                {
                    printf("Invalid queue depth!\n");
                    return -1;
                }
                break;

            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (!userUrls.empty())
    {
        if (userUrls.size() == 1)
        {
            rtspUrls.fill(userUrls[0]);
        }
        else
        {
            size_t copyCount = std::min(userUrls.size(), rtspUrls.size());
            for (size_t i = 0; i < copyCount; ++i)
            {
                rtspUrls[i] = userUrls[i];
            }

            if (userUrls.size() < rtspUrls.size())
            {
                for (size_t i = userUrls.size(); i < rtspUrls.size(); ++i)
                {
                    rtspUrls[i] = userUrls.back();
                }

                printf("Only %zu RTSP URL(s) provided. Remaining channels reuse the last URL.\n", userUrls.size());
            }
            else if (userUrls.size() > rtspUrls.size())
            {
                printf("Provided %zu RTSP URL(s); only the first %zu will be used.\n", userUrls.size(), rtspUrls.size());
            }
        }
    }

    printf("========== Runtime Config ==========\n");
    for (size_t i = 0; i < rtspUrls.size(); ++i)
    {
        printf("RTSP URL[%zu] : %s\n", i, rtspUrls[i].c_str());
    }
    printf("Queue Depth : %d (per channel)\n", queueDepth);
    printf("Display Chn : %d\n", DISP_CHN_NUM);
    printf("====================================\n");

    // Alloc buffers for every display channel.
    for (int i = 0; i < DISP_CHN_NUM; i++)
    {
        my_buffer::getInstance()->AllocVideoBuffer(i, 1920 * 1088 * 3 / 2);
    }

    // Init OpenGL singleton.
    CT507Graphics::getInstance();

    for (int i = 0; i < DISP_CHN_NUM; ++i)
    {
        g_vdecNodes[i] = new t507_vdec_node(i);
        if (g_vdecNodes[i]->create() == -1)
        {
            printf("Failed to create decoder for channel %d\n", i);
            return 1;
        }

        g_h264Queues[i] = new H264Queue(queueDepth);
        g_pullers[i] = new rtspPuller(rtspUrls[i].c_str(), g_h264Queues[i]);

        g_decodeCtx[i].chn = i;
        g_decodeCtx[i].vdecNode = g_vdecNodes[i];
        g_decodeCtx[i].h264Queue = g_h264Queues[i];
    }

    for (int i = 0; i < DISP_CHN_NUM; ++i)
    {
        std::thread t([i] {
            g_pullers[i]->loop();
        });
        t.detach();
    }

    logInfo("puller loop started, start Opengl\n");
    startOpengl();

#ifdef FILE_TEST
    startChn();
#endif

    while (!bThreadShouldStop)
    {
        usleep(1000 * 1000);
    }

    for (int i = 0; i < DISP_CHN_NUM; ++i)
    {
        if (g_h264Queues[i] != nullptr)
        {
            g_h264Queues[i]->stop();
        }
    }

    return 0;
}