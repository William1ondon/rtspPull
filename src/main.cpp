#include "live555.h"
#include <thread>

int main(int argc, char** argv) {
	rtspPuller puller("rtsp://192.168.88.88/mainstream");
    puller.loop();
    return 0;
}