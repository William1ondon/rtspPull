#ifndef LIVE555_H
#define LIVE555_H

#include <mutex>
#include <condition_variable>
#include <vector>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "media_frame.h"
#include "t507_vdec.h"

struct H264Packet {
    uint8_t* data;
    size_t   size;
    bool     isIDR;
};

class H264Queue {
public:
    explicit H264Queue(size_t maxDepth)
        : mMaxDepth(maxDepth) {}

    bool push(uint8_t* data, size_t size, bool isIDR) {
        std::lock_guard<std::mutex> lk(mMutex);

        // 队列满了：直接丢（防止卡死）
        if (mQueue.size() >= mMaxDepth) {
            delete[] data;
            return false;
        }

        mQueue.push({data, size, isIDR});
        mCond.notify_one();
        return true;
    }

    bool pop(H264Packet& pkt) {
        std::unique_lock<std::mutex> lk(mMutex);
        mCond.wait(lk, [&] { return !mQueue.empty() || mStop; });

        if (mQueue.empty())
            return false;

        pkt = mQueue.front();
        mQueue.pop();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lk(mMutex);
        mStop = true;
        mCond.notify_all();
    }

private:
    std::queue<H264Packet> mQueue;
    std::mutex mMutex;
    std::condition_variable mCond;
    size_t mMaxDepth;
    bool mStop{false};
};


struct Frame {
    std::vector<uint8_t> data;
    uint64_t pts;
};

class FrameBuffer {
public:
    void push(uint8_t* data, uint32_t size, uint64_t pts) {
        std::unique_lock<std::mutex> lock(mtx);
        frame.data.assign(data, data + size);
        frame.pts = pts;
        hasFrame = true;
        cv.notify_one();
    }

    bool pop(Frame& out) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return hasFrame; });
        out = frame;
        hasFrame = false;
        return true;
    }

private:
    Frame frame;
    bool hasFrame{false};
    std::mutex mtx;
    std::condition_variable cv;
};

class StreamClientState {
public:
  StreamClientState();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};


class rtspPuller {
public:
	rtspPuller(const char* rtspURL, H264Queue* h264queue); 
	~rtspPuller(){};
  void loop();

private:
  bool tryConnect();

public:
  TaskScheduler* scheduler = nullptr;
  UsageEnvironment* env = nullptr;

	FrameBuffer frameBuffer;
  const char* rtspURL;
  int reconnectIntervalMs;
  int reconnectMaxTimes;
  int reconnectCount = 0;
  StreamClientState scs;

  H264Queue* h264Queue;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient: public RTSPClient {
public:
  	static ourRTSPClient* createNew(UsageEnvironment& env, 
                                    const char* rtspURL,
                                    StreamClientState* scs,
                                    rtspPuller* owner,
                                    int verbosityLevel = 0,
                                    char const* applicationName = NULL,
                                    portNumBits tunnelOverHTTPPortNum = 0);

protected:
	ourRTSPClient(UsageEnvironment& env, 
                const char* rtspURL, 
                StreamClientState* scs,
                rtspPuller* owner,
			          int verbosityLevel, 
                const char* applicationName, 
                portNumBits tunnelOverHTTPPortNum);
		// called only by createNew();
	virtual ~ourRTSPClient();

public:
  StreamClientState* scs;
	rtspPuller* owner;
};

class MyH264Sink : public MediaSink {
public:
  static MyH264Sink* createNew(UsageEnvironment& env,
                                MediaSubsession& subsession,
                                H264Queue& queue,
                                unsigned bufferSize = 512000,
                                FrameBuffer* fb = NULL);

private:
  MyH264Sink(UsageEnvironment& env, MediaSubsession& subsession, H264Queue& queue, unsigned bufferSize, FrameBuffer* fb);
    // called only by "createNew()"
  virtual ~MyH264Sink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
				                        struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			 struct timeval presentationTime, unsigned durationInMicroseconds);

  static void onSourceClosure(void* clientData);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  unsigned fBufferSize;
  FrameBuffer* fFrameBuffer;
  Boolean fWrittenSPSPPS;
  H264Queue& fQueue;

  uint8_t* fSPS = nullptr;
  unsigned fSPSLen = 0;

  uint8_t* fPPS = nullptr;
  unsigned fPPSLen = 0;

  uint8_t* fSEI = nullptr;
  unsigned fSEILen = 0;

  bool fHaveSPS = false;
  bool fHavePPS = false;
  bool fHaveSEI = false;

};

#endif // LIVE555_H