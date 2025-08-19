// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <thread>

#define IS_GEMINI_435LE(pid) (pid == 0x0815)

int main(void) try {

    // Create a pipeline with default device.
    ob::Pipeline pipe;

    // This example only supports Gemini 435Le device
    auto device = pipe.getDevice();
    if(!IS_GEMINI_435LE(device->getDeviceInfo()->getPid())) {
        std::cout << "This example only supports Gemini 435Le device." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    // By creating config to configure which streams to enable or disable for the pipeline, here the depth stream will be enabled.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    // Enable depth stream.
    config->enableVideoStream(OB_STREAM_DEPTH);

    // Enable confidence stream.  The resolution and fps of confidence must match depth stream.
    auto enabledProfiles = config->getEnabledStreamProfileList();
    if(enabledProfiles) {
        for(uint32_t i = 0; i < enabledProfiles->getCount(); i++) {
            auto profile = enabledProfiles->getProfile(i);
            if(profile && profile->getType() == OB_STREAM_DEPTH) {
                auto depthProfile = profile->as<ob::VideoStreamProfile>();
                if(depthProfile) {
                    config->enableVideoStream(OB_STREAM_CONFIDENCE, depthProfile->getWidth(), depthProfile->getHeight(), depthProfile->getFps());
                }
                break;
            }
        }
    }
    // Start the pipeline with config.
    pipe.start(config);

    // Create a window for rendering, and set the resolution of the window.
    ob_smpl::CVWindow win("Confidence", 1280, 720, ob_smpl::ARRANGE_GRID);

    while(win.run()) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }
        // Render frame in the wisndow.
        win.pushFramesToView(frameSet);
    }

    // Stop the pipeline
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
