#include "live555.h"
#include <thread>

int main(int argc, char** argv) {
	static int count = 0;
	rtspPuller puller(argv[1]);

	std::thread t([&] { puller.loop(); });
	
	while (1) {
        // Frame frame;
        // puller.frameBuffer.pop(frame);

        // printf("main got frame: size=%zu pts=%lu\n",
        //        frame.data.size(), frame.pts);

        // writeCacheToFile(frame.data.data(), frame.data.size(), "outputRtsp.h264");

		count++;
		if (count > 1000)
		{
			break;
		}
		
		usleep(10 * 1000); // 10ms
    }
	return 0;
}