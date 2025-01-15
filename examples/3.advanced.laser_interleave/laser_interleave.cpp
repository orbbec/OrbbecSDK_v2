// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

void inputWatcher();

std::shared_ptr<ob::Filter>        postDepthFilter         = nullptr;
std::shared_ptr<ob::Filter>        postLeftInfraredFilter  = nullptr;
std::shared_ptr<ob::Filter>        postRightInfraredFilter = nullptr;
std::shared_ptr<ob_smpl::CVWindow> win;

int  main(void) try {
    // Create a pipeline with default device
    ob::Pipeline pipe;

    // Get the device from the pipeline
    auto device = pipe.getDevice();

    // Check if the device supports frame interleave
    if(!device->isFrameInterleaveSupported()) {
        std::cerr << "Current default device does not support frame interleave" << std::endl;
        std::cout << "Press any key to exit...";
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    // Configure which streams to enable or disable for the Pipeline by creating a Config
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    // enable depth stream with default profile
    config->enableVideoStream(OB_STREAM_DEPTH);
    config->enableVideoStream(OB_STREAM_IR_LEFT);
    config->enableVideoStream(OB_STREAM_IR_RIGHT);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

    // Create SequenceIdFilter post processor to filter frames.
    // The SequenceIdFilter also supports processing of infrared frames.
    postDepthFilter         = ob::FilterFactory::createFilter("SequenceIdFilter");
    postLeftInfraredFilter  = ob::FilterFactory::createFilter("SequenceIdFilter");
    postRightInfraredFilter = ob::FilterFactory::createFilter("SequenceIdFilter");
    
    // load frame interleave mode as 'Laser On-Off'
    device->loadFrameInterleave("Laser On-Off");
    // enable frame interleave
    device->setBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, true);

    // The default parameters were loaded when loadFrameInterleave is called
    // You can also modify these parameters yourself
    //
    // 1. frame interleave parameters for index 0(index starts from 0):
    // device->setIntProperty(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, 0);
    // device->setIntProperty(OB_PROP_LASER_CONTROL_INT, 1);  // first: set laser control to 1 to turn on laser
    // device->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, 3000);
    // device->setIntProperty(OB_PROP_DEPTH_GAIN_INT, 16);
    // device->setIntProperty(OB_PROP_IR_BRIGHTNESS_INT, 60);
    // device->setIntProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, 30000);

    // 2. frame interleave parameters for index 1(index starts from 0):
    // device->setIntProperty(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, 1);
    // device->setIntProperty(OB_PROP_LASER_CONTROL_INT, 0);  // second: set laser control to 0 to turn off laser
    // device->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, 3000);
    // device->setIntProperty(OB_PROP_DEPTH_GAIN_INT, 16);
    // device->setIntProperty(OB_PROP_IR_BRIGHTNESS_INT, 60);
    // device->setIntProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, 17000);

    // Start the pipeline with config
    pipe.start(config);

    postDepthFilter->setConfigValue("sequenceid", -1); // sequenceid can be -1,0,1
    postLeftInfraredFilter->setConfigValue("sequenceid", -1);
    postRightInfraredFilter->setConfigValue("sequenceid", -1);

    auto inputWatchThread = std::thread(inputWatcher);
    inputWatchThread.detach();

    // Create a window for rendering and set the resolution of the window

    // create window for render
    win = std::make_shared<ob_smpl::CVWindow>("Laser On-Off", 1280, 720, ob_smpl::ARRANGE_GRID);
    while(win->run()) {
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        auto postFilter = [](std::shared_ptr<ob::FrameSet> frameSet, std::shared_ptr<ob::Filter> &filter, OBFrameType frameType) -> std::shared_ptr<ob::Frame> {
            auto tempFrame = frameSet->getFrame(frameType);
            if(!tempFrame) {
                return nullptr;
            }
            return filter->process(tempFrame);
        };

        try {
            // Using SequenceId filter to filter frames
            
            // 1: depth
            auto depthFrame = postFilter(frameSet, postDepthFilter, OB_FRAME_DEPTH);
            if(depthFrame) {
                // add frame to render queue
                win->pushFramesToView(depthFrame, 0);
            }

            // 2: left infrared
            auto leftIrFrame = postFilter(frameSet, postLeftInfraredFilter, OB_FRAME_IR_LEFT);
            if(leftIrFrame) {
                // add frame to render queue
                win->pushFramesToView(leftIrFrame, 1);
            }

            // 2: right infrared
            auto rightIrFrame = postFilter(frameSet, postRightInfraredFilter, OB_FRAME_IR_RIGHT);
            if(rightIrFrame) {
                // add frame to render queue
                win->pushFramesToView(rightIrFrame, 2);
            }
        }
        catch(ob::Error &e) {
            std::cerr << "SequenceIdFilter error: " << e.what() << std::endl;
        }
    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    // close frame interleave
    device->setBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, false);

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit...";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

void printCommandTips() {
    std::cout << "\n-------------------------------";
    std::cout << "\nCommand usage: <filter> <param>";
    std::cout << "\n  <filter>: stream filter name, must be one of the following values:";
    std::cout << "\n            depth";
    std::cout << "\n            left_ir";
    std::cout << "\n            right_ir";
    std::cout << "\n  <param>:  stream filter param, must be one of the following values:";
    std::cout << "\n            all: disable sequenceid filter";
    std::cout << "\n            0: set sequenceid to 0";
    std::cout << "\n            1: set sequenceid to 1";
    std::cout << "\nPress 'q' or 'quit' to exit the program." << std::endl;
}

void inputWatcher() {
    while(true) {
        std::string cmd;

        printCommandTips();
        std::getline(std::cin, cmd);
        if(cmd == "quit" || cmd == "q") {
            win->close();
            break;
        }
        else {
            std::istringstream       ss(cmd);
            std::string              tmp;
            std::vector<std::string> controlVec;
            while(ss >> tmp) {
                controlVec.push_back(tmp);
            }

            if(controlVec.size() != 2) {
                std::cerr << "Error: invalid param." << std::endl;
                continue;
            }

            // filter
            std::shared_ptr<ob::Filter> filter = nullptr;
            if(controlVec.at(0) == "depth") {
                filter = postDepthFilter;
            }
            else if(controlVec.at(0) == "left_ir") {
                filter = postLeftInfraredFilter;
            }
            else if(controlVec.at(0) == "right_ir") {
                filter = postRightInfraredFilter;
            }
            else {
                std::cerr << "Error: invalid param." << std::endl;
                continue;
            }

            // param
            int32_t sequenceid = 0;

            if(controlVec.at(1) == "all") {
                sequenceid = -1;
            }
            else if(controlVec.at(1) == "0") {
                sequenceid = 0;
            }
            else if(controlVec.at(1) == "1") {
                sequenceid = 1;
            }
            else {
                std::cerr << "Error: invalid param." << std::endl;
                continue;
            }

            // set filter
            try {
                filter->setConfigValue("sequenceid", sequenceid);
                std::cout << "Set sequenceid successfully" << std::endl;
            }
            catch(ob::Error &e) {
                std::cerr << "Set sequenceid error: " << e.what() << std::endl;
            }
        }
    }
}