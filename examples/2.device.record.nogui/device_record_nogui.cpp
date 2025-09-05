// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>

#define IS_ASTRA_MINI_DEVICE(pid) (pid == 0x069d || pid == 0x065b || pid == 0x065e)

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

    // Activate device clock synchronization
    try {
        device->timerSyncWithHost();
    }
    catch(ob::Error &e) {
        std::cerr << "Function: " << e.getFunction() << "\nArgs: " << e.getArgs() << "\nMessage: " << e.what() << "\nException Type: " << e.getExceptionType()
                  << std::endl;
    }

    // Create a config and enable all streams
    std::shared_ptr<ob::Config> config     = std::make_shared<ob::Config>();
    auto                        sensorList = device->getSensorList();
    auto                        count      = sensorList->getCount();
    for(uint32_t i = 0; i < count; i++) {
        auto sensor      = sensorList->getSensor(i);
        auto sensorType  = sensor->getType();
        auto profileList = sensor->getStreamProfileList();  // Get profileList to create Sensor object in advance
        if(IS_ASTRA_MINI_DEVICE(device->getDeviceInfo()->getPid())) {
            if(sensorType == OB_SENSOR_IR) {
                continue;
            }
        }
        config->enableStream(sensorType);
    }

    std::mutex                      frameMutex;
    std::map<OBFrameType, uint64_t> frameCountMap;
    pipe->start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        if(frameSet == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(frameMutex);
        auto                        count = frameSet->getCount();
        for(uint32_t i = 0; i < count; i++) {
            auto frame = frameSet->getFrameByIndex(i);
            if(frame) {
                auto type = frame->getType();
                frameCountMap[type]++;
            }
        }
    });

    // Initialize recording device with output file
    auto     startTime    = ob_smpl::getNowTimesMs();
    uint32_t waitTime     = 1000;
    auto     recordDevice = std::make_shared<ob::RecordDevice>(device, filePath);

    // operation prompt
    std::cout << "Streams and recorder have started!" << std::endl;
    std::cout << "Press ESC, 'q', or 'Q' to stop recording and exit safely." << std::endl;
    std::cout << "IMPORTANT: Always use ESC/q/Q to stop! Otherwise, the bag file will be corrupted and unplayable." << std::endl << std::endl;

    do {
        auto key = ob_smpl::waitForKeyPressed(waitTime);
        if(key == ESC_KEY || key == 'q' || key == 'Q') {
            break;
        }
        auto currentTime = ob_smpl::getNowTimesMs();
        if(currentTime > startTime + waitTime) {
            std::map<OBFrameType, uint64_t> tempCountMap;
            uint64_t                        duration;
            {
                // Copy data
                std::lock_guard<std::mutex> lock(frameMutex);

                // get time again
                currentTime = ob_smpl::getNowTimesMs();
                duration    = currentTime - startTime;
                if(!frameCountMap.empty()) {
                    startTime    = currentTime;
                    waitTime     = 2000;  // Change to 2s for next time
                    tempCountMap = frameCountMap;
                    for(auto &item: frameCountMap) {
                        item.second = 0; // reset count
                    }
                }
            }

            std::string seperate = "";
            if(tempCountMap.empty()) {
                std::cout << "Recording... Current FPS: 0" << std::endl;
            }
            else {
                std::cout << "Recording... Current FPS: ";
                for(const auto &item: tempCountMap) {
                    auto  name = ob::TypeHelper::convertOBFrameTypeToString(item.first);
                    float rate = item.second / (duration / 1000.0f);

                    std::cout << std::fixed << std::setprecision(2) << std::showpoint;
                    std::cout << seperate << name << "=" << rate;
                    seperate = ", ";
                }
                std::cout << std::endl;
            }
        }
    } while(true);

    // stop the pipeline
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
