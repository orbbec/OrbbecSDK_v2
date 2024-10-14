// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <mutex>
#include <thread>

bool enable_align_mode = 0;

// key press event processing
void handleKeyPress(ob_smpl::CVWindow &win, std::shared_ptr<ob::Pipeline> pipe, int key, std::shared_ptr<ob::Config> config) {
    if(key == 't' || key == 'T') {
        // Press the T key to switch align mode
        enable_align_mode = !enable_align_mode;

        // update the align mode in the config
        if(enable_align_mode) {
            config->setAlignMode(ALIGN_D2C_HW_MODE);
            win.addLog("Haeware Depth to Color Align: Enabled");
        }
        else {
            config->setAlignMode(ALIGN_DISABLE);
            win.addLog("Haeware Depth to Color Align: Disabled");
        }

        // restart the pipeline with the new config
        pipe->stop();
        pipe->start(config);
    }
}

std::shared_ptr<ob::Config> createHwD2CAlignConfig(std::shared_ptr<ob::Pipeline> pipe) {
    auto coloStreamProfiles = pipe->getStreamProfileList(OB_SENSOR_COLOR);
    auto count              = coloStreamProfiles->getCount();
    for(uint32_t i = 0; i < count; i++) {
        auto colorProfile                      = coloStreamProfiles->getProfile(i);
        // RGB is used here for easy rendering
        if (colorProfile->getFormat() != OB_FORMAT_RGB) {
            continue;
        }

        auto hwD2CSupportedDepthStreamProfiles = pipe->getD2CDepthProfileList(colorProfile, ALIGN_D2C_HW_MODE);
        if(hwD2CSupportedDepthStreamProfiles->count() == 0) {
            continue;
        }

        auto hwD2CAlignConfig = std::make_shared<ob::Config>();
        auto depthProfile     = hwD2CSupportedDepthStreamProfiles->getProfile(0);
        hwD2CAlignConfig->enableStream(colorProfile);                                                     // enable color stream
        hwD2CAlignConfig->enableStream(depthProfile);                                                     // enable depth stream
        hwD2CAlignConfig->setAlignMode(ALIGN_D2C_HW_MODE);                                                // enable hardware depth-to-color alignment
        hwD2CAlignConfig->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);  // output frameset with all types of frames
        return hwD2CAlignConfig;
    }
    return nullptr;
}

int main(void) try {
    // Create a pipeline with default device to manage stream
    auto pipe = std::make_shared<ob::Pipeline>();

    // enable frame sync inside the pipeline, which is synchronized by frame timestamp
    pipe->enableFrameSync();

    // Create a config for hardware depth-to-color alignment
    auto config = createHwD2CAlignConfig(pipe);
    if(config == nullptr) {
        std::cerr << "Current device does not support hardware depth-to-color alignment." << std::endl;
        return 1;
    }

    // Start the pipeline with config
    pipe->start(config);

    // Create a window for rendering and set the resolution of the window
    ob_smpl::CVWindow win("Hardware Depth to Color Align", 1280, 720, ob_smpl::ARRANGE_OVERLAY);
    // set key prompt
    win.setKeyPrompt("'T': Enable/Disable HwD2C, '+/-': Adjust Transparency");
    // set the callback function for the window to handle key press events
    win.setKeyPressedCallback([&](int key) { handleKeyPress(win, pipe, key, config); });

    while(win.run()) {
        // Wait for a frameset from the pipeline
        auto frameSet = pipe->waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }
        win.pushFramesToView(frameSet);
    }

    // Stop the Pipeline, no frame data will be generated
    pipe->stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

