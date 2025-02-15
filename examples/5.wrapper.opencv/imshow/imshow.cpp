// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "../common/cvhelper.hpp"

#include <memory>
#include <iostream>

int main(void) try {
    // Configure which streams to enable or disable for the Pipeline by creating a Config
    auto config = std::make_shared<ob::Config>();

    // enable depth stream with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);

    std::shared_ptr<ob::Pipeline> pipeline = std::make_shared<ob::Pipeline>();
    pipeline->start(config);

    while(true) {
        auto frames     = pipeline->waitForFrames();
        auto depthFrame = frames->getFrame(OB_FRAME_DEPTH);

        // Color mapping the depth data in order to visualize it intuitively
        cv::Mat depthVisualized = CvHelper::colorizeDepth(depthFrame);

        cv::imshow("ImageShow", depthVisualized);
        int key = cv::waitKey(1);
        if(key == 27 || key == 'q' || key == 'Q') {
            break;
        }
    }

    pipeline->stop();

    exit(EXIT_SUCCESS);
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}