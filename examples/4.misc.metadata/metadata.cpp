// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/ObTypes.h>

#include "utils.hpp"

#include <chrono>
#include <thread>
#include <iomanip>
#include <atomic>

std::thread inputWatchThread;
std::atomic<bool> isRunning(true);
void        inputWatcher();
const char *metadataTypeToString(OBFrameMetadataType type);
const char *frameTypeToString(OBFrameType type);

int main() try {
    // Create a pipeline.
    ob::Pipeline pipe;

    // Start the pipeline with default config.
    // Modify the default configuration by the configuration file: "OrbbecSDKConfig.xml"
    pipe.start();

    // Get key input
    inputWatchThread = std::thread(inputWatcher);
    inputWatchThread.detach();

    while(isRunning) {
        // Wait for frameSet from the pipeline, the default timeout is 1000ms.
        auto frameSet = pipe.waitForFrameset();
        if(!frameSet) {
            continue;
        }

        // Get the count of frames in the frameSet
        auto frameCount = frameSet->getCount();

        for(uint32_t i = 0; i < frameCount; i++) {
            // Get the frame from frameSet
            auto frame      = frameSet->getFrame(i);
            auto frameIndex = frame->index();
            // Get the metadata of the frame, and print the metadata every 30 frames
            if(frameIndex % 30 == 0) {
                std::cout << std::endl;
                std::cout << "frame    type: " << frameTypeToString(frame->type()) << std::endl;
                for(uint32_t j = 0; j < static_cast<uint32_t>(OB_FRAME_METADATA_TYPE_COUNT); j++) {
                    // If the frame has the metadata, get the metadata value
                    if(frame->hasMetadata(static_cast<OBFrameMetadataType>(j))) {
                        std::cout << "metadata type: " << std::left << std::setw(50) << metadataTypeToString(static_cast<OBFrameMetadataType>(j))
                                  << " metadata value: " << frame->getMetadataValue(static_cast<OBFrameMetadataType>(j)) << std::endl;
                    }
                }
            }
        }
    }
    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

void inputWatcher() {
    char input = ob_smpl::waitForKeyPressed();
    if(input == ESC_KEY) {
        isRunning = false;
    }
}

const char *metadataTypeToString(OBFrameMetadataType type) {
    switch(type) {
    case OB_FRAME_METADATA_TYPE_TIMESTAMP:
        return "OB_FRAME_METADATA_TYPE_TIMESTAMP";
    case OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP:
        return "OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP";
    case OB_FRAME_METADATA_TYPE_FRAME_NUMBER:
        return "OB_FRAME_METADATA_TYPE_FRAME_NUMBER";
    case OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE:
        return "OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE";
    case OB_FRAME_METADATA_TYPE_EXPOSURE:
        return "OB_FRAME_METADATA_TYPE_EXPOSURE";
    case OB_FRAME_METADATA_TYPE_GAIN:
        return "OB_FRAME_METADATA_TYPE_GAIN";
    case OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE:
        return "OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE";
    case OB_FRAME_METADATA_TYPE_WHITE_BALANCE:
        return "OB_FRAME_METADATA_TYPE_WHITE_BALANCE";
    case OB_FRAME_METADATA_TYPE_BRIGHTNESS:
        return "OB_FRAME_METADATA_TYPE_BRIGHTNESS";
    case OB_FRAME_METADATA_TYPE_CONTRAST:
        return "OB_FRAME_METADATA_TYPE_CONTRAST";
    case OB_FRAME_METADATA_TYPE_SATURATION:
        return "OB_FRAME_METADATA_TYPE_SATURATION";
    case OB_FRAME_METADATA_TYPE_SHARPNESS:
        return "OB_FRAME_METADATA_TYPE_SHARPNESS";
    case OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION:
        return "OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION";
    case OB_FRAME_METADATA_TYPE_HUE:
        return "OB_FRAME_METADATA_TYPE_HUE";
    case OB_FRAME_METADATA_TYPE_GAMMA:
        return "OB_FRAME_METADATA_TYPE_GAMMA";
    case OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY:
        return "OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY";
    case OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION:
        return "OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION";
    case OB_FRAME_METADATA_TYPE_MANUAL_WHITE_BALANCE:
        return "OB_FRAME_METADATA_TYPE_MANUAL_WHITE_BALANCE";
    case OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE:
        return "OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE";
    case OB_FRAME_METADATA_TYPE_FRAME_RATE:
        return "OB_FRAME_METADATA_TYPE_FRAME_RATE";
    case OB_FRAME_METADATA_TYPE_AE_ROI_LEFT:
        return "OB_FRAME_METADATA_TYPE_AE_ROI_LEFT";
    case OB_FRAME_METADATA_TYPE_AE_ROI_TOP:
        return "OB_FRAME_METADATA_TYPE_AE_ROI_TOP";
    case OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT:
        return "OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT";
    case OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM:
        return "OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM";
    case OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY:
        return "OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY";
    case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME:
        return "OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME";
    case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE:
        return "OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE";
    case OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX:
        return "OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX";
    case OB_FRAME_METADATA_TYPE_LASER_POWER:
        return "OB_FRAME_METADATA_TYPE_LASER_POWER";
    case OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL:
        return "OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL";
    case OB_FRAME_METADATA_TYPE_LASER_STATUS:
        return "OB_FRAME_METADATA_TYPE_LASER_STATUS";
    case OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA:
        return "OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA";
    case OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_OFFSET:
        return "OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_OFFSET";
    case OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE:
        return "OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE";
    default:
        return "unknown metadata type";
    }
}

const char *frameTypeToString(OBFrameType type) {
    switch(type) {
    case OB_FRAME_VIDEO:
        return "OB_FRAME_VIDEO";
    case OB_FRAME_IR:
        return "OB_FRAME_IR";
    case OB_FRAME_COLOR:
        return "OB_FRAME_COLOR";
    case OB_FRAME_DEPTH:
        return "OB_FRAME_DEPTH";
    case OB_FRAME_ACCEL:
        return "OB_FRAME_ACCEL";
    case OB_FRAME_GYRO:
        return "OB_FRAME_GYRO";
    case OB_FRAME_IR_LEFT:
        return "OB_FRAME_IR_LEFT";
    case OB_FRAME_IR_RIGHT:
        return "OB_FRAME_IR_RIGHT";
    default:
        return "unknown frame type";
    }
}