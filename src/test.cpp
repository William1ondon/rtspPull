#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

static const uint8_t kStartCode[4] = {0x00,0x00,0x00,0x01};

static void writeSPSPPS(MediaSubsession& subsession, FILE* fp) {
    char const* sprop = subsession.fmtp_spropparametersets();
    if (!sprop) return;

    unsigned numSPropRecords;
    SPropRecord* spropRecords = parseSPropParameterSets(sprop, numSPropRecords);

    for (unsigned i = 0; i < numSPropRecords; ++i) {
        fwrite(kStartCode, 1, 4, fp);
        fwrite(spropRecords[i].sPropBytes, 1, spropRecords[i].sPropLength, fp);
    }

    delete[] spropRecords;
}

// ---------------------------
//  per-stream state
// ---------------------------
class StreamClientState {
public:
    StreamClientState() :
        iter(nullptr), session(nullptr), subsession(nullptr),
        streamTimerTask(nullptr), duration(0.0) {}

    virtual ~StreamClientState() {
        if (iter) delete iter;
        if (session) {
            Medium::close(session);
        }
    }

public:
    MediaSubsessionIterator* iter;
    MediaSession* session;
    MediaSubsession* subsession;
    TaskToken streamTimerTask;
    double duration;
};

// ---------------------------
//  RTSPClient subclass
// ---------------------------
class MyRTSPClient : public RTSPClient {
public:
    static MyRTSPClient* createNew(UsageEnvironment& env,
                                   const char* rtspURL,
                                   StreamClientState* scs,
                                   int verbosityLevel = 0,
                                   const char* applicationName = NULL,
                                   portNumBits tunnelOverHTTPPortNum = 0)
    {
        return new MyRTSPClient(env, rtspURL, scs, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
    }

protected:
    MyRTSPClient(UsageEnvironment& env, const char* rtspURL,
                 StreamClientState* scs,
                 int verbosityLevel, const char* applicationName,
                 portNumBits tunnelOverHTTPPortNum)
        : RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1),
          scs(scs) {}

    virtual ~MyRTSPClient() {}

public:
    StreamClientState* scs;
};

// ---------------------------
//  H264 sink
// ---------------------------
class MyH264Sink : public MediaSink {
public:
    static MyH264Sink* createNew(UsageEnvironment& env,
                                 MediaSubsession& subsession,
                                 FILE* outFp,
                                 unsigned bufferSize = 512000)
    {
        return new MyH264Sink(env, subsession, outFp, bufferSize);
    }

private:
    MyH264Sink(UsageEnvironment& env, MediaSubsession& subsession, FILE* outFp, unsigned bufferSize)
        : MediaSink(env), fSubsession(subsession), fOutFp(outFp),
          fBufferSize(bufferSize), fWrittenSPSPPS(False)
    {
        fReceiveBuffer = new uint8_t[fBufferSize];
    }

    virtual ~MyH264Sink() {
        delete[] fReceiveBuffer;
    }

    virtual Boolean continuePlaying() override {
        if (!fSource) return False;

        fSource->getNextFrame(fReceiveBuffer, fBufferSize,
                              afterGettingFrame, this,
                              onSourceClosure, this);
        return True;
    }

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds)
    {
        MyH264Sink* sink = (MyH264Sink*)clientData;
        sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
    }

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime,
                           unsigned durationInMicroseconds)
    {
        if (numTruncatedBytes > 0) {
            // buffer too small, you can enlarge bufferSize
        }

        // 写 SPS/PPS（只能写一次）
        if (!fWrittenSPSPPS) {
            writeSPSPPS(fSubsession, fOutFp);
            fWrittenSPSPPS = True;
        }

        // 写 NALU（Annex-B）
        fwrite(kStartCode, 1, 4, fOutFp);
        fwrite(fReceiveBuffer, 1, frameSize, fOutFp);
        fflush(fOutFp);

        continuePlaying();
    }

    static void onSourceClosure(void* clientData) {
        MyH264Sink* sink = (MyH264Sink*)clientData;
        sink->stopPlaying();
    }

private:
    uint8_t* fReceiveBuffer;
    MediaSubsession& fSubsession;
    FILE* fOutFp;
    unsigned fBufferSize;
    Boolean fWrittenSPSPPS;
};

// ---------------------------
//  global callbacks
// ---------------------------
static void subsessionAfterPlaying(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

    Medium::close(subsession->sink);
    subsession->sink = nullptr;
}

static void subsessionByeHandler(void* clientData) {
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

    UsageEnvironment& env = rtspClient->envir();
    env << "Received RTCP BYE on \"" << subsession->mediumName() << "\"\n";

    subsessionAfterPlaying(clientData);
}

static void streamTimerHandler(void* clientData) {
    MyRTSPClient* rtspClient = (MyRTSPClient*)clientData;
    StreamClientState* scs = rtspClient->scs;

    UsageEnvironment& env = rtspClient->envir();
    env << "Stream timeout, closing session\n";

    Medium::close(rtspClient);
    rtspClient = nullptr;
}

// ---------------------------
//  continueAfterDESCRIBE
// ---------------------------
static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    UsageEnvironment& env = rtspClient->envir();
    MyRTSPClient* client = (MyRTSPClient*)rtspClient;
    StreamClientState* scs = client->scs;

    if (resultCode != 0) {
        env << "DESCRIBE failed: " << resultString << "\n";
        delete[] resultString;
        return;
    }

    char* sdpDescription = resultString;
    scs->session = MediaSession::createNew(env, sdpDescription);

    if (!scs->session) {
        env << "Failed to create MediaSession\n";
        return;
    }

    scs->iter = new MediaSubsessionIterator(*scs->session);
    scs->subsession = scs->iter->next();

    if (!scs->subsession) {
        env << "No subsession found\n";
        return;
    }

    // only handle first subsession (video)
    if (strcmp(scs->subsession->mediumName(), "video") != 0) {
        env << "Not video\n";
        return;
    }

    if (!scs->subsession->initiate()) {
        env << "Failed to initiate subsession\n";
        return;
    }

    // open output file
    FILE* outFp = fopen("output.h264", "wb");
    if (!outFp) {
        env << "Failed to open output file\n";
        return;
    }

    // create sink
    scs->subsession->sink = MyH264Sink::createNew(env, *scs->subsession, outFp);

    if (!scs->subsession->sink) {
        env << "Failed to create sink\n";
        return;
    }

    scs->subsession->sink->startPlaying(*(scs->subsession->readSource()),
                                        subsessionAfterPlaying,
                                        scs->subsession);

    // send SETUP
    rtspClient->sendSetupCommand(*scs->subsession, [](RTSPClient* rtspClient, int resultCode, char* resultString) {
        UsageEnvironment& env = rtspClient->envir();
        if (resultCode != 0) {
            env << "SETUP failed: " << resultString << "\n";
            delete[] resultString;
            return;
        }
        delete[] resultString;

        // PLAY
        rtspClient->sendPlayCommand(*((MyRTSPClient*)rtspClient)->scs->session, [](RTSPClient* rtspClient, int resultCode, char* resultString) {
            UsageEnvironment& env = rtspClient->envir();
            if (resultCode != 0) {
                env << "PLAY failed: " << resultString << "\n";
                delete[] resultString;
                return;
            }
            env << "PLAY started\n";
            delete[] resultString;
        });
    });

    delete[] resultString;
}

// ---------------------------
//  rtspPuller
// ---------------------------
class rtspPuller {
public:
    rtspPuller(const char* url) : rtspURL(url), reconnectIntervalMs(2000), reconnectMaxTimes(10) {}

    void loop() {
        while (true) {
            if (tryConnect()) {
                // 进入 live555 事件循环（阻塞）
                scheduler->doEventLoop();
            }

            // 断开后自动重连
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnectIntervalMs));
            reconnectCount++;
            if (reconnectCount >= reconnectMaxTimes) break;
        }
    }

private:
    bool tryConnect() {
    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    MyRTSPClient* rtspClient = MyRTSPClient::createNew(*env, rtspURL, &scs, 1, "rtspPuller");

    if (!rtspClient) {
        std::cerr << "Failed to create RTSPClient\n";
        return false;
    }

    rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
    return true;
}

private:
    const char* rtspURL;
    StreamClientState scs;

    TaskScheduler* scheduler = nullptr;
    UsageEnvironment* env = nullptr;

    int reconnectIntervalMs;
    int reconnectMaxTimes;
    int reconnectCount = 0;
};

int main() {
    rtspPuller puller("rtsp://192.168.88.88/mainstream");
    puller.loop();
    return 0;
}
