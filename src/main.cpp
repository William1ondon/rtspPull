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
#include <sys/syscall.h>
#include <time.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <memory>
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

static uint64_t monotonicTimeUs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

static void accumulatePerfDuration(uint64_t elapsedUs, uint64_t& totalUs, uint64_t& maxUs)
{
    totalUs += elapsedUs;
    if (elapsedUs > maxUs)
    {
        maxUs = elapsedUs;
    }
}

struct PipePerfWindow
{
    uint64_t windowStartUs{0};
    size_t popCount{0};
    size_t sendCount{0};
    size_t sendFailCount{0};
    size_t loadCount{0};
    size_t nullFrameCount{0};
    size_t preIdrDropCount{0};
    size_t resyncDropCount{0};
    size_t idrCount{0};
    size_t queueDepthMax{0};
    size_t inputBytes{0};
    size_t outputBytes{0};
    uint64_t popWaitUsTotal{0};
    uint64_t popWaitUsMax{0};
    uint64_t sendUsTotal{0};
    uint64_t sendUsMax{0};
    uint64_t getFrameUsTotal{0};
    uint64_t getFrameUsMax{0};
    uint64_t loadUsTotal{0};
    uint64_t loadUsMax{0};
    uint64_t loopUsTotal{0};
    uint64_t loopUsMax{0};
};

static void maybePrintPipePerf(int chn, PipePerfWindow& perf, bool force = false)
{
    (void)chn;
    const uint64_t nowUs = monotonicTimeUs();
    if (perf.windowStartUs == 0)
    {
        perf.windowStartUs = nowUs;
    }

    const uint64_t elapsedUs = nowUs - perf.windowStartUs;
    if (!force && elapsedUs < 1000000ULL)
    {
        return;
    }

    perf = PipePerfWindow{};
    perf.windowStartUs = nowUs;
}

static void removeDumpFile(const std::string& filename)
{
    ::remove(filename.c_str());
}

static void clearDumpFiles()
{
    for (int i = 0; i < DISP_CHN_NUM; ++i)
    {
        removeDumpFile("chn" + std::to_string(i) + ".h264");
        removeDumpFile("pre_queue_chn" + std::to_string(i) + ".h264");
    }
}

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

static int readBit(const uint8_t* data, size_t bitLen, size_t& bitPos)
{
    if (bitPos >= bitLen)
    {
        return -1;
    }

    int bit = (data[bitPos / 8] >> (7 - (bitPos % 8))) & 0x01;
    ++bitPos;
    return bit;
}

static int readUnsignedExpGolomb(const uint8_t* data, size_t bitLen, size_t& bitPos)
{
    size_t leadingZeroBits = 0;

    while (true)
    {
        int bit = readBit(data, bitLen, bitPos);
        if (bit < 0)
        {
            return -1;
        }
        if (bit == 1)
        {
            break;
        }

        ++leadingZeroBits;
        if (leadingZeroBits > 31)
        {
            return -1;
        }
    }

    unsigned value = 1;
    for (size_t i = 0; i < leadingZeroBits; ++i)
    {
        int bit = readBit(data, bitLen, bitPos);
        if (bit < 0)
        {
            return -1;
        }
        value = (value << 1) | static_cast<unsigned>(bit);
    }

    return static_cast<int>(value - 1);
}

static int parseFirstMbInSliceNoStartCode(const uint8_t* nal, size_t len)
{
    if (nal == nullptr || len < 2)
    {
        return -1;
    }

    uint8_t nalType = nal[0] & 0x1F;
    if (nalType != 1 && nalType != 5)
    {
        return -1;
    }

    const uint8_t* payload = nal + 1;
    size_t payloadLen = len - 1;
    std::vector<uint8_t> rbsp;
    rbsp.reserve(payloadLen);

    for (size_t i = 0; i < payloadLen; ++i)
    {
        if (i + 2 < payloadLen && payload[i] == 0x00 && payload[i + 1] == 0x00 && payload[i + 2] == 0x03)
        {
            rbsp.push_back(0x00);
            rbsp.push_back(0x00);
            i += 2;
            continue;
        }
        rbsp.push_back(payload[i]);
    }

    if (rbsp.empty())
    {
        return -1;
    }

    size_t bitPos = 0;
    return readUnsignedExpGolomb(rbsp.data(), rbsp.size() * 8, bitPos);
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
static int ifReceiveIDR[DISP_CHN_NUM] = {0};
static void* listen_body(void* arg)
{
    DecodeThreadContext* ctx = static_cast<DecodeThreadContext*>(arg);
    if (ctx == nullptr || ctx->vdecNode == nullptr || ctx->h264Queue == nullptr)
    {
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

    H264Packet pkt;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    bool decoderPrimed = false;
    bool decoderNeedResync = false;
    size_t decoderResyncDropped = 0;
    std::deque<std::string> recentDecHistory;
    constexpr size_t kRecentDecHistoryMax = 8;
    PipePerfWindow pipePerf;

    while (!bThreadShouldStop)
    {
        const uint64_t loopStartUs = monotonicTimeUs();
        auto finishLoop = [&](bool forceLog = false) {
            accumulatePerfDuration(monotonicTimeUs() - loopStartUs, pipePerf.loopUsTotal, pipePerf.loopUsMax);
            maybePrintPipePerf(chn_num, pipePerf, forceLog);
        };

        const uint64_t popWaitStartUs = monotonicTimeUs();
        if (!ctx->h264Queue->pop(pkt))
        {
            finishLoop(true);
            break;
        }
        ++pipePerf.popCount;
        accumulatePerfDuration(monotonicTimeUs() - popWaitStartUs, pipePerf.popWaitUsTotal, pipePerf.popWaitUsMax);

        if (pkt.data == nullptr || pkt.size == 0)
        {
            delete[] pkt.data;
            finishLoop();
            continue;
        }

        const uint8_t nalType = pkt.data[0] & 0x1F;
        int firstMbInSlice = parseFirstMbInSliceNoStartCode(pkt.data, pkt.size);

        if (nalType == 7)
        {
            sps.assign(pkt.data, pkt.data + pkt.size);
            delete[] pkt.data;
            finishLoop();
            continue;
        }

        if (nalType == 8)
        {
            pps.assign(pkt.data, pkt.data + pkt.size);
            delete[] pkt.data;
            finishLoop();
            continue;
        }

        // Skip standalone non-VCL NALs (SEI/AUD/etc.); cedarc only needs SPS/PPS plus slice NALs here.
        if (nalType != 1 && nalType != 5)
        {
            delete[] pkt.data;
            finishLoop();
            continue;
        }

        // SPS/PPS now stay cached in the queue object, so slice packets can keep the queue shallow.
        if (pkt.isIDR || sps.empty() || pps.empty())
        {
            ctx->h264Queue->copyLatestParameterSets(sps, pps);
        }

        // Drop non-IDR slices until the decoder has been primed, or while waiting for a clean resync point.
        if ((!decoderPrimed || decoderNeedResync) && !pkt.isIDR)
        {
            if (decoderNeedResync)
            {
                ++decoderResyncDropped;
                ++pipePerf.resyncDropCount;
            }
            else
            {
                ++pipePerf.preIdrDropCount;
            }
            delete[] pkt.data;
            finishLoop();
            continue;
        }

        const size_t queueDepthBeforeSend = ctx->h264Queue->depth();
        pipePerf.queueDepthMax = std::max(pipePerf.queueDepthMax, queueDepthBeforeSend);
        size_t outSize = 4 + pkt.size;
        bool needExtra = false;

        if (pkt.isIDR && !sps.empty() && !pps.empty())
        {
            outSize += 4 + sps.size();
            outSize += 4 + pps.size();
            needExtra = true;
        }

        char decHistoryBuf[256] = {0};
        snprintf(decHistoryBuf, sizeof(decHistoryBuf),
                 "q=%zu nal=%u pkt=%zu out=%zu idr=%d first_mb=%d primed=%d sps=%zu pps=%zu extra=%d",
                 queueDepthBeforeSend,
                 static_cast<unsigned>(nalType),
                 pkt.size,
                 outSize,
                 pkt.isIDR ? 1 : 0,
                 firstMbInSlice,
                 decoderPrimed ? 1 : 0,
                 sps.size(),
                 pps.size(),
                 needExtra ? 1 : 0);
        recentDecHistory.emplace_back(decHistoryBuf);
        while (recentDecHistory.size() > kRecentDecHistoryMax)
        {
            recentDecHistory.pop_front();
        }

        pipePerf.inputBytes += pkt.size;
        pipePerf.outputBytes += outSize;
        if (pkt.isIDR)
        {
            ++pipePerf.idrCount;
        }

        const uint64_t sendStartUs = monotonicTimeUs();
        int sendRet = ctx->vdecNode->sendH264FrameDirect(pkt.data, pkt.size, sps, pps, pkt.isIDR, 0);
        delete[] pkt.data;
        ++pipePerf.sendCount;
        accumulatePerfDuration(monotonicTimeUs() - sendStartUs, pipePerf.sendUsTotal, pipePerf.sendUsMax);

        if (sendRet != 0)
        {
            ++pipePerf.sendFailCount;
            if (!decoderNeedResync)
            {
                decoderResyncDropped = 0;
            }
            decoderNeedResync = true;
            decoderPrimed = false;
            finishLoop();
            continue;
        }

        if (pkt.isIDR && decoderNeedResync)
        {
            decoderNeedResync = false;
            decoderPrimed = true;
            decoderResyncDropped = 0;
        }
        else if (pkt.isIDR && !decoderPrimed)
        {
            decoderPrimed = true;
        }

        const uint64_t getFrameStartUs = monotonicTimeUs();
        media_frame* tempFrame = ctx->vdecNode->getFrame();
        accumulatePerfDuration(monotonicTimeUs() - getFrameStartUs, pipePerf.getFrameUsTotal, pipePerf.getFrameUsMax);
        if (tempFrame == nullptr)
        {
            ++pipePerf.nullFrameCount;
            finishLoop();
            continue;
        }

        void* virAddr = nullptr;
        tempFrame->lockPacket(0, &virAddr, NULL);
        if (virAddr != nullptr)
        {
            const uint64_t loadStartUs = monotonicTimeUs();
            CT507Graphics::getInstance()->load_texture(virAddr, IMAGEWIDTH, IMAGEHEIGHT, chn_num);
            accumulatePerfDuration(monotonicTimeUs() - loadStartUs, pipePerf.loadUsTotal, pipePerf.loadUsMax);
            ++pipePerf.loadCount;
        }

        finishLoop();
    }

    maybePrintPipePerf(chn_num, pipePerf, true);
    return nullptr;
}

bool startChn(size_t activeChnCount)
{
    pthread_t ret = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (size_t i = 0; i < activeChnCount; ++i)
    {
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
    (void)prog;
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
    std::vector<std::string> rtspUrls;
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
                    return -1;
                }
                break;

            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (userUrls.empty())
    {
        rtspUrls.emplace_back(defaultUrl);
    }
    else
    {
        size_t activeCount = std::min(userUrls.size(), static_cast<size_t>(DISP_CHN_NUM));
        rtspUrls.assign(userUrls.begin(), userUrls.begin() + activeCount);
    }

    const size_t activeStreamCount = rtspUrls.size();

    clearDumpFiles();

    // Alloc buffers for every display channel.
    const size_t bufferHeight = static_cast<size_t>((IMAGEHEIGHT + 15) & ~15);
    const size_t yPlaneSize = static_cast<size_t>(IMAGEWIDTH) * bufferHeight;
    const size_t extBufferSize = yPlaneSize + yPlaneSize / 2;

    for (int i = 0; i < DISP_CHN_NUM; i++)
    {
        if (my_buffer::getInstance()->AllocVideoBuffer(i, static_cast<int>(extBufferSize)) != 0)
        {
            return 1;
        }
    }

    // Init OpenGL singleton.
    CT507Graphics::getInstance();

    for (size_t i = 0; i < activeStreamCount; ++i)
    {
        g_vdecNodes[i] = new t507_vdec_node(static_cast<int>(i));
        if (g_vdecNodes[i]->create() == -1)
        {
            return 1;
        }

        g_h264Queues[i] = new H264Queue(static_cast<int>(i), queueDepth);
        g_pullers[i] = new rtspPuller(rtspUrls[i].c_str(), g_h264Queues[i], static_cast<int>(i));

        g_decodeCtx[i].chn = static_cast<int>(i);
        g_decodeCtx[i].vdecNode = g_vdecNodes[i];
        g_decodeCtx[i].h264Queue = g_h264Queues[i];
    }

    for (size_t i = 0; i < activeStreamCount; ++i)
    {
        std::thread t([i] {

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % 4, &cpuset);

            pthread_setaffinity_np(
                pthread_self(),
                sizeof(cpu_set_t),
                &cpuset
            );

            pid_t tid = syscall(SYS_gettid);
            (void)tid;

            g_pullers[i]->loop();
        });

        t.detach();
    }

    startOpengl();

#ifdef FILE_TEST
    startChn(activeStreamCount);
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
