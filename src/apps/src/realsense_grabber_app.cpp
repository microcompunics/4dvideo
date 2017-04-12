#include <thread>

#include <util/consumer.hpp>
#include <util/tiny_logger.hpp>

#include <realsense/realsense_grabber.hpp>


int main()
{
    FrameQueue writerQueue, visualizerQueue;

    std::thread grabberThread([&]()
    {
        RealsenseGrabber grabber;
        //grabber.addQueue(&writerQueue);
        grabber.addQueue(&visualizerQueue);
        grabber.init();
        grabber.run();
    });

    std::thread visualizerThread([&]()
    {
        Consumer<FrameQueue> visualizer(visualizerQueue);
        visualizer.run();
    });

    grabberThread.join();

    return EXIT_SUCCESS;
}