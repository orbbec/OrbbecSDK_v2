// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <mutex>
#include <thread>
#include <atomic>

std::atomic<bool> isPaused{ false };

int main(void) try {

    std::cout << "Please enter the output filename (with .bag extension) and press Enter to start recording: ";
    std::string filePath;
    std::getline(std::cin, filePath);

    // Create a context, for getting devices and sensors
    std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();

    // Query device list
    auto deviceList = context->queryDeviceList();
    if(deviceList->getCount() < 1) {
        std::cout << "No device found! Please connect a supported device and retry this program." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        exit(EXIT_FAILURE);
    }

    // Acquire first available device
    auto device = deviceList->getDevice(0);

    // Create a pipeline the specified device
    auto pipe = std::make_shared<ob::Pipeline>(device);

    // Create a config and enable all streams
    std::shared_ptr<ob::Config> config     = std::make_shared<ob::Config>();
    auto                        sensorList = device->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);
        config->enableStream(sensorType);
    }

    std::mutex frameMutex;
    uint32_t   frameCount = 0;

    pipe->start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        if(frameCount % 20 == 0) {
            for(uint32_t i = 0; i < frameSet->getCount(); i++) {
                auto frame = frameSet->getFrame(i);
                if(!frame) {
                    continue;
                }

                std::cout << "frame index: " << frame->getIndex() << ", tsp: " << frame->getTimeStampUs()
                          << ", format: " << ob::TypeHelper::convertOBFormatTypeToString(frame->getFormat()) << std::endl;
            }
        }
        frameCount++;
    });

    // Initialize recording device with output file
    auto recordDevice = std::make_shared<ob::RecordDevice>(device, filePath);
    while(true) {
        auto key = ob_smpl::waitForKeyPressed(1);
        if(key == ESC_KEY) {  // Esc key to exit.
            break;
        }
        else if(key == 'p' || key == 'P') {  // Press 'p' or 'P' to pause/resume recording.
            if(isPaused) {
                recordDevice->resume();
                isPaused.store(false);
                std::cout << "Recording resumed." << std::endl;
            }
            else {
                recordDevice->pause();
                isPaused.store(true);
                std::cout << "Recording paused." << std::endl;
            }
        }
    }

    pipe->stop();

    // Flush and save recording file
    recordDevice = nullptr;

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "Function: " << e.getFunction() << "\nArgs: " << e.getArgs() << "\nMessage: " << e.what() << "\nException Type: " << e.getExceptionType()
              << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
