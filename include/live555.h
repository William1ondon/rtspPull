#ifndef LIVE555_H
#define LIVE555_H

#include <mutex>
#include <condition_variable>
#include <cstdio>
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
    long long pts;
};

class H264Queue {
public:
    H264Queue(int chn, size_t maxDepth)
        : mChn(chn), mMaxDepth(maxDepth) {}

    void copyLatestParameterSets(std::vector<uint8_t>& sps, std::vector<uint8_t>& pps) {
        std::lock_guard<std::mutex> lk(mMutex);
        if (!mLatestSps.empty()) {
            sps = mLatestSps;
        }
        if (!mLatestPps.empty()) {
            pps = mLatestPps;
        }
    }

    size_t depth() {
        std::lock_guard<std::mutex> lk(mMutex);
        return mQueue.size();
    }

    bool push(uint8_t* data, size_t size, bool isIDR, long long pts) {
        std::lock_guard<std::mutex> lk(mMutex);

        if (mStop) {
            delete[] data;
            return false;
        }

        const uint8_t nalType = (data != nullptr && size > 0) ? (data[0] & 0x1F) : 0;
        const bool isSpsOrPps = (nalType == 7 || nalType == 8);
        const bool isSei = (nalType == 6);

        if (nalType == 7) {
            mLatestSps.assign(data, data + size);
            delete[] data;
            return true;
        }

        if (nalType == 8) {
            mLatestPps.assign(data, data + size);
            delete[] data;
            return true;
        }

        if (isSei) {
            delete[] data;
            return true;
        }

        // Once overflow happens, flush broken backlog and wait for the next clean sync point.
        if (mQueue.size() >= mMaxDepth) {
            printf("[queue] overflow flush chn=%d depth=%zu max=%zu incoming_nal=%u incoming_size=%zu isIDR=%d isSpsOrPps=%d isSei=%d pts=%lld\n",
                   mChn,
                   mQueue.size(),
                   mMaxDepth,
                   static_cast<unsigned>(nalType),
                   size,
                   isIDR ? 1 : 0,
                   isSpsOrPps ? 1 : 0,
                   isSei ? 1 : 0,
                   pts);
            while (!mQueue.empty()) {
                H264Packet old = mQueue.front();
                mQueue.pop();
                delete[] old.data;
            }
            mNeedIdr = true;
            mDroppedWhileWaitingIdr = 0;
        }

        if (mNeedIdr) {
            if (isIDR) {
                printf("[queue] resync chn=%d dropped=%zu incoming_size=%zu pts=%lld\n",
                       mChn,
                       mDroppedWhileWaitingIdr,
                       size,
                       pts);
                mNeedIdr = false;
                mDroppedWhileWaitingIdr = 0;
            } else if (!isSpsOrPps && !isSei) {
                ++mDroppedWhileWaitingIdr;
                delete[] data;
                return false;
            }
        }

        mQueue.push({data, size, isIDR, pts});
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
        mNeedIdr = false;
        mDroppedWhileWaitingIdr = 0;
        mLatestSps.clear();
        mLatestPps.clear();
        while (!mQueue.empty()) {
            H264Packet old = mQueue.front();
            mQueue.pop();
            delete[] old.data;
        }
        mCond.notify_all();
    }

private:
    std::queue<H264Packet> mQueue;
    std::mutex mMutex;
    std::condition_variable mCond;
    int mChn;
    size_t mMaxDepth;
    bool mNeedIdr{false};
    size_t mDroppedWhileWaitingIdr{0};
    bool mStop{false};
    std::vector<uint8_t> mLatestSps;
    std::vector<uint8_t> mLatestPps;
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
	rtspPuller(const char* rtspURL, H264Queue* h264queue, int chn); 
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
  int chn = -1;
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
                                int chn,
                                unsigned bufferSize = 512000,
                                FrameBuffer* fb = NULL);

private:
  MyH264Sink(UsageEnvironment& env, MediaSubsession& subsession, H264Queue& queue, int chn, unsigned bufferSize, FrameBuffer* fb);
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
  int fChannel;
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
  bool fPreQueueDumpStarted = false;

};

#endif // LIVE555_H
