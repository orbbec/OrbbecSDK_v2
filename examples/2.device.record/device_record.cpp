// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <mutex>
#include <thread>
#include <atomic>

#define IS_ASTRA_MINI_DEVICE(pid) (pid == 0x069d || pid == 0x065b || pid == 0x065e)

std::atomic<bool> isPaused{false};

void handleKeyPress(ob_smpl::CVWindow &win, std::shared_ptr<ob::RecordDevice> recorder, int key);

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
    std::shared_ptr<ob::Config>  config  = std::make_shared<ob::Config>();
    auto sensorList = device->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);
        if(IS_ASTRA_MINI_DEVICE(device->getDeviceInfo()->getPid())) {
            if(sensorType == OB_SENSOR_IR) {
                continue;
            }
        }
        config->enableStream(sensorType);
    }

    std::mutex                          frameMutex;
    std::shared_ptr<const ob::FrameSet> renderFrameSet;
    pipe->start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        std::lock_guard<std::mutex> lock(frameMutex);
        renderFrameSet = frameSet;
    });

    // Initialize recording device with output file
    auto recordDevice = std::make_shared<ob::RecordDevice>(device, filePath);
    std::cout << "Streams and recorder have started!" << std::endl;
    std::cout << "Press ESC to stop recording and exit safely." << std::endl;
    std::cout << "IMPORTANT: Always use ESC to stop! Otherwise, the bag file will be corrupted and unplayable." << std::endl << std::endl;

    ob_smpl::CVWindow win("Record", 1280, 720, ob_smpl::ARRANGE_GRID);

    win.setKeyPrompt("Press 'S' to pause/resume recording.");
    // set the callback function for the window to handle key press events
    win.setKeyPressedCallback([&win, recordDevice](int key) { handleKeyPress(win, recordDevice, key); });

    while(win.run()) {
        std::lock_guard<std::mutex> lock(frameMutex);
        if(renderFrameSet == nullptr) {
            continue;
        }
        win.pushFramesToView(renderFrameSet);
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

void handleKeyPress(ob_smpl::CVWindow& win, std::shared_ptr<ob::RecordDevice> recorder, int key) {
    if(key == 'S' || key == 's') {
        if(!isPaused) {
            recorder->pause();
            isPaused.store(true);
            win.addLog("[PAUSED] Recording paused");
		}
		else {
            recorder->resume();
            isPaused.store(false);
            win.addLog("[RESUMED] Recording resumed");
		}
    }
}
