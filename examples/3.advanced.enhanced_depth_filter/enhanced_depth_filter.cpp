// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <iostream>
#include <string>
#include <fstream>

// Parse command-line arguments.
// --model <path> (or -m <path>) specifies the inference model file used by the EnhancedDepthFilter;
// when omitted, the filter uses its default model file located next to the filter library.
// Returns false when the program should exit immediately (help requested or invalid argument).
static bool parseArguments(int argc, char **argv, std::string &modelPath) {
    const std::string usage = std::string("Usage: ") + argv[0] + " [--model <model_file_path>]";
    for(int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if((arg == "--model" || arg == "-m") && i + 1 < argc) {
            modelPath = argv[++i];
        }
        else if(arg == "-h" || arg == "--help") {
            std::cout << usage << std::endl;
            return false;
        }
        else {
            std::cout << "Unknown argument: " << arg << "\n" << usage << std::endl;
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv) try {
    std::string modelPath;
    if(!parseArguments(argc, argv, modelPath)) {
        return EXIT_FAILURE;
    }

    // When a model path is given, fail fast if it does not exist (an empty path lets the filter use its default).
    if(!modelPath.empty() && !std::ifstream(modelPath).good()) {
        std::cout << "model file not found: " << modelPath << std::endl;
        return EXIT_FAILURE;
    }

    ob::Pipeline pipe;
    auto         device = pipe.getDevice();

    // The EnhancedDepthFilter requires license authorization. Exit directly when the device does not support it.
    if(!device->isLicenseAuthorizationSupported()) {
        std::cout << "device does not support license authorization, exit." << std::endl;
        return EXIT_FAILURE;
    }

    // Read and print the license info before creating the filter.
    auto licenseInfo = device->readLicenseInfo();
    std::cout << "license info:" << licenseInfo << std::endl;

    // The EnhancedDepthFilter relies on an inference engine that is only available on NVIDIA Jetson
    // (Linux arm64). On any other platform, stop here after reading the license info.
#if defined(__ANDROID__) || !(defined(__linux__) && defined(__aarch64__))
    std::cout << "EnhancedDepthFilter is only supported on the NVIDIA Jetson platform, exit." << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return EXIT_FAILURE;
#else
    // Software align depth to color (D2C). Alternatively, you can use hardware D2C on the device
    // (e.g. enable it via the pipeline config) when the requested resolution is supported by hardware.
    auto alignFilter         = std::make_shared<ob::Align>(OB_STREAM_COLOR);
    auto enhancedDepthFilter = std::make_shared<ob::EnhancedDepthFilter>(device, modelPath);

    auto config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_COLOR, 640, 480, OB_FPS_ANY, OB_FORMAT_RGB);
    config->enableVideoStream(OB_STREAM_DEPTH, 640, 480, OB_FPS_ANY, OB_FORMAT_Y16);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    pipe.start(config);

    ob_smpl::CVWindow win("EnhancedDepthFilter", 1280, 720, ob_smpl::ARRANGE_GRID);

    while(win.run()) {
        auto frameset = pipe.waitForFrameset(1000);
        if(!frameset) {
            continue;
        }
        std::vector<std::shared_ptr<const ob::Frame>> originFrames{ frameset->getColorFrame(), frameset->getDepthFrame() };

        auto aligned = alignFilter->process(frameset);
        if(!aligned) {
            continue;
        }

        auto processed = enhancedDepthFilter->process(aligned);
        if(!processed || !processed->is<ob::FrameSet>()) {
            continue;
        }

        auto processedFrameset = processed->as<ob::FrameSet>();
        auto colorFrame        = processedFrameset->getColorFrame();
        auto depthFrame        = processedFrameset->getDepthFrame();
        auto confidenceFrame   = processedFrameset->getFrame(OB_FRAME_CONFIDENCE);

        std::vector<std::shared_ptr<const ob::Frame>> processedFrames{ colorFrame, depthFrame };

        if(confidenceFrame) {
            processedFrames.emplace_back(confidenceFrame);
        }
        win.pushFramesToView(originFrames, 0);
        win.pushFramesToView(processedFrames, 1);
    }

    pipe.stop();
    return 0;
#endif
}
catch(const ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return EXIT_FAILURE;
}
