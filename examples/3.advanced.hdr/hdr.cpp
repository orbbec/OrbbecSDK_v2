// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

int main(void) try {
    // Create a pipeline with default device
    ob::Pipeline pipe;

    // Get the device from the pipeline
    auto device = pipe.getDevice();

    // Check if the device supports HDR merge
    if(!device->isPropertySupported(OB_STRUCT_DEPTH_HDR_CONFIG, OB_PERMISSION_READ_WRITE)) {
        std::cerr << "Current default device does not support HDR merge" << std::endl;
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

    // Create HDRMerge post processor to merge depth frames betweens different hdr sequence ids.
    // The HDRMerge also supports processing of infrared frames.
    auto hdrMerge = ob::FilterFactory::createFilter("HDRMerge");
    if( device->isFrameInterleaveSupported() ) {
        // load frame interleave mode as 'Depth from HDR'
        device->loadFrameInterleave("Depth from HDR");
        // enable frame interleave mode
        device->setBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, true);

        // The default parameters were loaded when loadFrameInterleave is called
        // You can also modify these parameters yourself
        // 
        // 1. frame interleave parameters for index 0(index starts from 0):
        // device->setIntProperty(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, 0);
        // device->setIntProperty(OB_PROP_LASER_CONTROL_INT, 1);  // laser control must be 1
        // device->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, 7500);
        // device->setIntProperty(OB_PROP_DEPTH_GAIN_INT, 16);
        // device->setIntProperty(OB_PROP_IR_BRIGHTNESS_INT, 60);
        // device->setIntProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, 10000);
        
        // 2. frame interleave parameters for index 1(index starts from 0):
        // device->setIntProperty(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, 1);
        // device->setIntProperty(OB_PROP_LASER_CONTROL_INT, 1); // laser control must be 1
        // device->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, 1);
        // device->setIntProperty(OB_PROP_DEPTH_GAIN_INT, 16);
        // device->setIntProperty(OB_PROP_IR_BRIGHTNESS_INT, 20);
        // device->setIntProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, 2000);
    }else {
        // configure and enable Hdr stream
        OBHdrConfig obHdrConfig;
        obHdrConfig.enable     = true;  // enable HDR merge
        obHdrConfig.exposure_1 = 7500;
        obHdrConfig.gain_1     = 24;
        obHdrConfig.exposure_2 = 100;
        obHdrConfig.gain_2     = 16;
        device->setStructuredData(OB_STRUCT_DEPTH_HDR_CONFIG, reinterpret_cast<uint8_t *>(&obHdrConfig), sizeof(OBHdrConfig));
    }

    // Start the pipeline with config
    pipe.start(config);

    // Create a window for rendering and set the resolution of the window
    ob_smpl::CVWindow win("HDR-Merge", 1280, 720, ob_smpl::ARRANGE_GRID);
    win.addLog("The HDR-Merged depth frames are displayed in the last row of the window.");
    while(win.run()) {
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        // Get the depth and infrared frames from the frameset
        auto depthFrame   = frameSet->getFrame(OB_FRAME_DEPTH)->as<ob::DepthFrame>();
        auto leftIRFrame  = frameSet->getFrame(OB_FRAME_IR_LEFT)->as<ob::IRFrame>();
        auto rightIRFrame = frameSet->getFrame(OB_FRAME_IR_RIGHT)->as<ob::IRFrame>();

        // Get the HDR sequence id from the depth frame metadata
        int groupId = static_cast<int>(depthFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX));
        win.pushFramesToView({ depthFrame, leftIRFrame, rightIRFrame }, groupId);

        try {
            // Using HDRMerge filter to merge hdr frames
            auto result = hdrMerge->process(frameSet);
            if(result == nullptr) {
                continue;
            }
            auto resultFrameSet   = result->as<ob::FrameSet>();
            auto resultDepthFrame = resultFrameSet->getFrame(OB_FRAME_DEPTH)->as<ob::DepthFrame>();

            // add merged depth frame to render queue
            win.pushFramesToView(resultDepthFrame, 10);  // set the group id to 10 to avoid same group id with original depth frame
        }
        catch(ob::Error &e) {
            std::cerr << "HDRMerge error: " << e.what() << std::endl;
        }
    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    // close hdr merge
    if(device->isFrameInterleaveSupported()) {
        device->setBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, false);
    }
    else {
        OBHdrConfig obHdrConfig = { 0 };
        obHdrConfig.enable = false;
        device->setStructuredData(OB_STRUCT_DEPTH_HDR_CONFIG, reinterpret_cast<uint8_t *>(&obHdrConfig), sizeof(OBHdrConfig));
    }
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit...";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

