#ifndef LIVE555_H
#define LIVE555_H

#include <mutex>
#include <condition_variable>
#include <vector>
#include <fstream>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

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

class rtspPuller {
public:
	rtspPuller(const char* rtspURL); 
	~rtspPuller(){};
  void loop();

private:
	void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

public:
	FrameBuffer frameBuffer;
  const char* rtspURL;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

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

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient: public RTSPClient {
public:
  	static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
				  rtspPuller* owner,
				  int verbosityLevel = 0,
				  char const* applicationName = NULL,
				  portNumBits tunnelOverHTTPPortNum = 0);

protected:
	ourRTSPClient(UsageEnvironment& env, char const* rtspURL, rtspPuller* owner,
			int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
		// called only by createNew();
	virtual ~ourRTSPClient();

public:
  	StreamClientState scs;
	rtspPuller* owner;
};

class DummySink: public MediaSink {
public:
  static DummySink* createNew(UsageEnvironment& env,
			      MediaSubsession& subsession, // identifies the kind of data that's being received
			      char const* streamId = NULL,  // identifies the stream itself (optional)
				  FrameBuffer* fb = NULL);

private:
  DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, FrameBuffer* fb);
    // called only by "createNew()"
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
				struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			 struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  char* fStreamId;
  FrameBuffer* fFrameBuffer;
  Boolean fWrittenSPSPPS;
  FILE* fOutFp;
};

#endif // LIVE555_H