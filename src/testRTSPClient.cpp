/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2015, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/

#include "live555.h"
static frame_shell *pFrameDec;
// Forward function definitions:

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")


// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

void usage(UsageEnvironment& env, char const* progName) {
  env << "Usage: " << progName << " <rtsp-url-1> ... <rtsp-url-N>\n";
  env << "\t(where each <rtsp-url-i> is a \"rtsp://\" URL)\n";
}

char eventLoopWatchVariable = 0;

rtspPuller::rtspPuller(const char* rtspURL, H264Queue* h264queue) : rtspURL(rtspURL), h264Queue(h264queue), reconnectIntervalMs(2000), reconnectMaxTimes(10)  {}

void rtspPuller::loop(){
  while(true){
    if(tryConnect()){
      logInfo("RTSP connected successfully, start eventloop\n");
      scheduler->doEventLoop();
    }

    // 断开后自动重连
    std::this_thread::sleep_for(std::chrono::milliseconds(reconnectIntervalMs));
    reconnectCount++;
    if (reconnectCount >= reconnectMaxTimes) break;
  }
}

bool rtspPuller::tryConnect() {
  scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  ourRTSPClient* rtspClient = ourRTSPClient::createNew(*env, rtspURL, &scs, this, 1, "rtspPuller");
  if (!rtspClient) {
      std::cerr << "Failed to create RTSPClient\n";
      return false;
  }
  
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
  return true;
}

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.


// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
    UsageEnvironment& env = rtspClient->envir();
    ourRTSPClient* client = (ourRTSPClient*)rtspClient;
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
    scs->subsession->sink = MyH264Sink::createNew(env, *scs->subsession, *client->owner->h264Queue);

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
        rtspClient->sendPlayCommand(*((ourRTSPClient*)rtspClient)->scs->session, [](RTSPClient* rtspClient, int resultCode, char* resultString) {
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

// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  // MediaSession& session = subsession->parentSession();
  // MediaSubsessionIterator iter(session);
  // while ((subsession = iter.next()) != NULL) {
  //   if (subsession->sink != NULL) return; // this subsession is still active
  // }

  // All subsessions' streams have now been closed, so shutdown the client:
  // shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;

  UsageEnvironment& env = rtspClient->envir(); // alias
  env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState* scs = rtspClient->scs;

  UsageEnvironment& env = rtspClient->envir();
  env << "Stream timeout, closing session\n";

  Medium::close(rtspClient);
  rtspClient = nullptr;
}



// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL, StreamClientState* scs, rtspPuller* owner,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, scs, owner, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL, StreamClientState* scs, rtspPuller* owner,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1), scs(scs), owner(owner) {
}

ourRTSPClient::~ourRTSPClient() {
}


// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
  if (iter) delete iter;
  if (session) {
      Medium::close(session);
  }
}


// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 512*1024

MyH264Sink* MyH264Sink::createNew(UsageEnvironment& env, MediaSubsession& subsession, H264Queue& queue, unsigned bufferSize, FrameBuffer* fb) {
  pFrameDec = new frame_shell;
  return new MyH264Sink(env, subsession, queue, bufferSize, fb);
}

MyH264Sink::MyH264Sink(UsageEnvironment& env, MediaSubsession& subsession, H264Queue& queue, unsigned bufferSize, FrameBuffer* fb)
  : MediaSink(env),
    fSubsession(subsession),
    fQueue(queue),
    fBufferSize(bufferSize),
    fFrameBuffer(fb),
    fWrittenSPSPPS(False) {
  fReceiveBuffer = new uint8_t[fBufferSize];
}

MyH264Sink::~MyH264Sink() {
  delete[] fReceiveBuffer;
}

void MyH264Sink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
  MyH264Sink* sink = (MyH264Sink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void MyH264Sink::onSourceClosure(void* clientData) {
  MyH264Sink* sink = (MyH264Sink*)clientData;
  sink->stopPlaying();
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 0

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

void MyH264Sink::afterGettingFrame(unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned /*duration*/) {

    // 1️⃣ 解析 NAL type
    uint8_t nalType = fReceiveBuffer[0] & 0x1F;
    bool isIDR = (nalType == 5);

    // 2️⃣ 拷贝一份数据（RTSP buffer 不能外用）
    uint8_t* copyBuf = new uint8_t[frameSize];
    memcpy(copyBuf, fReceiveBuffer, frameSize);

    // 3️⃣ 非阻塞入队（满了直接丢）
    fQueue.push(copyBuf, frameSize, isIDR);

    continuePlaying();
}


Boolean MyH264Sink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, fBufferSize,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}
