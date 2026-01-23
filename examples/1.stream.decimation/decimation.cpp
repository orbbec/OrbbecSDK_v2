// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"
#include "utils_opencv.hpp"
#include <iostream>
#include <limits>

uint32_t getUserInput(uint32_t maxIndex) {
    int selected = -1;
    while(true) {
        std::cout << "Please input the index (0 - " << maxIndex - 1 << "): ";
        std::cin >> selected;

        if(std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input, please enter a number." << std::endl;
            continue;
        }

        if(selected < 0 || static_cast<uint32_t>(selected) >= maxIndex) {
            std::cout << "Index out of range. Please try again." << std::endl;
            continue;
        }

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        break;
    }
    return static_cast<uint32_t>(selected);
}

// Prints the details of a stream profile (Resolution, FPS, Format).
void printStreamProfile(const std::shared_ptr<ob::StreamProfile> &profile, uint32_t index) {
    auto videoProfile = profile->as<ob::VideoStreamProfile>();
    if(!videoProfile)
        return;

    auto formatName       = profile->getFormat();
    auto width            = videoProfile->getWidth();
    auto height           = videoProfile->getHeight();
    auto fps              = videoProfile->getFps();
    auto decimationConfig = videoProfile->getDecimationConfig();

    std::cout << index << ". format: " << ob::TypeHelper::convertOBFormatTypeToString(formatName) << ", res: " << width << "*" << height << ", fps: " << fps;

    // If decimation is active, print origin details
    if(decimationConfig.factor != 0) {
        std::cout << ", originRes: " << decimationConfig.originWidth << "*" << decimationConfig.originHeight
                  << ", decimation factor: " << decimationConfig.factor;
    }
    std::cout << std::endl;
}

// Enumerates and sets preset resolution configurations.
// Used primarily for specific devices like Gemini 435Le.
void enumeratePresetResolutionConfig(std::shared_ptr<ob::Device> device) {
    std::cout << "[Notice]: This device requires a Preset Resolution Configuration." << std::endl;
    std::cout << "Preset resolution config list: " << std::endl;

    auto presetList = device->getAvailablePresetResolutionConfigList();
    auto presetNum  = presetList->getCount();

    if(!presetList || presetNum == 0) {
        std::cerr << "No preset resolution config available" << std::endl;
        return;
    }

    // List all available presets
    for(uint32_t index = 0; index < presetNum; index++) {
        auto presetConfig = presetList->getPresetResolutionRatioConfig(index);
        std::cout << index << ". width " << presetConfig.width << ", height " << presetConfig.height << ", IR decimation " << presetConfig.irDecimationFactor
                  << ", Depth decimation " << presetConfig.depthDecimationFactor << std::endl;
    }

    // Get User Selection
    uint32_t selected = getUserInput(presetNum);

    // Apply the configuration to the device
    auto presetConfig = presetList->getPresetResolutionRatioConfig(selected);
    device->setStructuredData(OB_STRUCT_PRESET_RESOLUTION_CONFIG, (uint8_t *)&presetConfig, sizeof(presetConfig));
    std::cout << "Preset configuration applied." << std::endl;
}

// Displays available stream profiles for a sensor and lets the user select one.
std::shared_ptr<ob::StreamProfile> selectStreamProfile(const std::shared_ptr<ob::Sensor> &sensor, OBSensorType sensorType) {
    auto streamList = sensor->getStreamProfileList();
    if(streamList->getCount() == 0) {
        std::cerr << "No stream profiles available for this sensor." << std::endl;
        return nullptr;
    }

    std::cout << "Available profiles for " << ob::TypeHelper::convertOBSensorTypeToString(sensorType) << ":" << std::endl;
    for(uint32_t i = 0; i < streamList->getCount(); ++i) {
        printStreamProfile(streamList->getProfile(i), i);
    }

    // Special hint for IR sensors regarding synchronization
    if(isIRSensor(sensorType)) {
        std::cout << "[Note]: Please keep the original resolution and decimation factor consistent with Depth sensor." << std::endl;
    }

    uint32_t selected = getUserInput(streamList->getCount());

    return streamList->getProfile(selected);
}

int main() try {
    // Create a pipeline with default device.
    ob::Pipeline pipe;

    // Get the device with the pipeline
    std::shared_ptr<ob::Device> device = pipe.getDevice();

    if(!device) {
        std::cerr << "No device found!" << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }
    bool isPropertySupported = device->isPropertySupported(OB_STRUCT_PRESET_RESOLUTION_CONFIG, OB_PERMISSION_READ_WRITE);
    if(!isPropertySupported) {
        std::cout << "The device does not support preset resolution configuration." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    // Retrieve device info VID/PID
    auto deviceInfo = device->getDeviceInfo();
    auto vid        = deviceInfo->getVid();
    auto pid        = deviceInfo->getPid();

    std::cout << "Device connected: " << deviceInfo->getName() << " (VID: " << std::hex << vid << ", PID: " << pid << std::dec << ")" << std::endl;
    // Specific check for Gemini 305 device
    if(!ob_smpl::isGemini305Device(vid, pid)) {
        enumeratePresetResolutionConfig(device);
    }

    // Configure Sensors and Streams
    std::shared_ptr<ob::SensorList> sensorList = device->getSensorList();
    std::shared_ptr<ob::Config>     config     = std::make_shared<ob::Config>();

    std::cout << "Configuring Sensors..." << std::endl;

    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        OBSensorType sensorType = sensorList->getSensorType(index);

        if(isIRSensor(sensorType) || sensorType == OB_SENSOR_DEPTH) {
            auto sensor = sensorList->getSensor(index);
            std::cout << "\n[Sensor " << index << "]: " << ob::TypeHelper::convertOBSensorTypeToString(sensorType) << std::endl;

            // User selects a profile (Resolution/FPS)
            std::shared_ptr<ob::StreamProfile> profile = selectStreamProfile(sensor, sensorType);

            if(profile) {
                // Enable this specific stream profile in the pipeline configuration
                config->enableStream(profile);
                std::cout << " -> Stream enabled." << std::endl;

                // support 305 decimation configuration to stream profile enabling
                // auto videoProfile     = profile->as<ob::VideoStreamProfile>();
                // auto decimationConfig = videoProfile->getDecimationConfig();
                // config->enableVideoStream(sensorType, decimationConfig);
            }
        }
    }

    // Start the Pipeline
    std::cout << "\nStarting Pipeline..." << std::endl;
    pipe.start(config);

    // Create a visualization window using the helper class (OpenCV based)
    ob_smpl::CVWindow win("Infrared/Depth Viewer", 1280, 800, ob_smpl::ARRANGE_GRID);
    std::cout << "Pipeline started. Rendering... (Press ESC to close)" << std::endl;

    while(win.run()) {
        auto frameSet = pipe.waitForFrameset(100);

        if(frameSet != nullptr) {
            // Render the frames to the window
            win.pushFramesToView(frameSet);
        }
    }

    // Stop the pipeline to release hardware resources.
    pipe.stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}