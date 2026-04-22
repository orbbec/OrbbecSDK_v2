// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"
#include "utils_opencv.hpp"
#include <iostream>
#include <limits>
#include <vector>

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

// Check if a resolution (width x height) already exists in the list.
bool hasResolution(const std::vector<OBPresetResolutionConfig> &list, int16_t width, int16_t height) {
    for(auto &cfg: list) {
        if(cfg.width == width && cfg.height == height) {
            return true;
        }
    }
    return false;
}

// Step 1: List unique origin resolutions from preset configs.
OBPresetResolutionConfig selectOriginResolution(std::shared_ptr<ob::Device> device, std::vector<OBPresetResolutionConfig> &matchedPresets) {
    auto presetList = device->getAvailablePresetResolutionConfigList();
    auto presetNum  = presetList->getCount();

    if(!presetList || presetNum == 0) {
        std::cerr << "No preset resolution config available." << std::endl;
        return {};
    }

    // Collect one representative preset per unique origin resolution
    std::vector<OBPresetResolutionConfig> uniqueResolutions;
    for(uint32_t i = 0; i < presetNum; i++) {
        auto cfg = presetList->getPresetResolutionRatioConfig(i);
        if(!hasResolution(uniqueResolutions, cfg.width, cfg.height)) {
            uniqueResolutions.push_back(cfg);
        }
    }

    std::cout << "\n[Step 1] Select origin resolution:" << std::endl;
    for(size_t i = 0; i < uniqueResolutions.size(); i++) {
        std::cout << "  " << i << ". " << uniqueResolutions[i].width << "x" << uniqueResolutions[i].height << std::endl;
    }

    uint32_t selected = getUserInput(static_cast<uint32_t>(uniqueResolutions.size()));
    auto     chosen   = uniqueResolutions[selected];

    // Collect all preset configs matching this resolution
    for(uint32_t i = 0; i < presetNum; i++) {
        auto cfg = presetList->getPresetResolutionRatioConfig(i);
        if(cfg.width == chosen.width && cfg.height == chosen.height) {
            matchedPresets.push_back(cfg);
        }
    }

    return chosen;
}

// Step 2: Pick a decimation config for the selected resolution.
OBPresetResolutionConfig selectDecimation(const std::vector<OBPresetResolutionConfig> &presets) {
    if(presets.size() == 1) {
        auto &cfg = presets[0];
        std::cout << "\n[Step 2] Decimation auto-selected: Depth=" << cfg.depthDecimationFactor << ", IR=" << cfg.irDecimationFactor << std::endl;
        return cfg;
    }

    std::cout << "\n[Step 2] Select decimation factor:" << std::endl;
    for(size_t i = 0; i < presets.size(); i++) {
        auto &cfg = presets[i];
        std::cout << "  " << i << ". Depth=" << cfg.depthDecimationFactor << ", IR=" << cfg.irDecimationFactor << std::endl;
    }

    return presets[getUserInput(static_cast<uint32_t>(presets.size()))];
}

// Find the first profile that matches the given origin resolution and decimation factor.
// For 305 (factor > 0): match by decimationConfig.originWidth/originHeight.
// For 435Le (factor == 0): match by profile width/height == originWidth/factor.
std::shared_ptr<ob::StreamProfile> findProfile(const std::shared_ptr<ob::Sensor> &sensor, uint32_t originWidth, uint32_t originHeight,
                                               uint32_t decimationFactor) {
    auto     profiles     = sensor->getStreamProfileList();
    uint32_t outputWidth  = originWidth / decimationFactor;
    uint32_t outputHeight = originHeight / decimationFactor;
    for(uint32_t i = 0; i < profiles->getCount(); i++) {
        auto videoProfile     = profiles->getProfile(i)->as<ob::VideoStreamProfile>();
        auto decimationConfig = videoProfile->getDecimationConfig();
        if((decimationConfig.factor > 0 && decimationConfig.originWidth == originWidth && decimationConfig.originHeight == originHeight)
           || (decimationConfig.factor == 0 && videoProfile->getWidth() == outputWidth && videoProfile->getHeight() == outputHeight)) {
            return profiles->getProfile(i);
        }
    }
    return nullptr;
}

int main() try {
    // Create a pipeline with the default device.
    ob::Pipeline pipe;

    auto device = pipe.getDevice();
    if(!device) {
        std::cerr << "No device found!" << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    // Check if the device supports preset resolution configuration.
    if(!device->isPropertySupported(OB_STRUCT_PRESET_RESOLUTION_CONFIG, OB_PERMISSION_READ_WRITE)) {
        std::cout << "This device does not support preset resolution configuration." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    auto deviceInfo = device->getDeviceInfo();
    auto vid        = deviceInfo->getVid();
    auto pid        = deviceInfo->getPid();
    std::cout << "Device: " << deviceInfo->getName() << " (VID: 0x" << std::hex << vid << ", PID: 0x" << pid << std::dec << ")" << std::endl;

    // Step 1: Select origin resolution (deduplicated from preset configs).
    std::vector<OBPresetResolutionConfig> matchedPresets;
    OBPresetResolutionConfig              originPreset = selectOriginResolution(device, matchedPresets);

    // Step 2: Select decimation factor for the chosen resolution.
    OBPresetResolutionConfig presetConfig = selectDecimation(matchedPresets);

    // Apply the preset config (Gemini 305 uses a different mechanism).
    if(!ob_smpl::isGemini305Device(vid, pid)) {
        device->setStructuredData(OB_STRUCT_PRESET_RESOLUTION_CONFIG, (uint8_t *)&presetConfig, sizeof(presetConfig));
        std::cout << "Preset configuration applied." << std::endl;
    }

    uint32_t originWidth  = static_cast<uint32_t>(originPreset.width);
    uint32_t originHeight = static_cast<uint32_t>(originPreset.height);

    // Locate Depth, IR Left, and IR Right sensors.
    auto                        sensorList = device->getSensorList();
    auto                        config     = std::make_shared<ob::Config>();
    std::shared_ptr<ob::Sensor> depthSensor;
    std::shared_ptr<ob::Sensor> irLeftSensor;
    std::shared_ptr<ob::Sensor> irRightSensor;

    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        OBSensorType type = sensorList->getSensorType(i);
        if(type == OB_SENSOR_DEPTH) {
            depthSensor = sensorList->getSensor(i);
        }
        else if(type == OB_SENSOR_IR_LEFT) {
            irLeftSensor = sensorList->getSensor(i);
        }
        else if(type == OB_SENSOR_IR_RIGHT) {
            irRightSensor = sensorList->getSensor(i);
        }
    }

    // Enable streams.
    if(ob_smpl::isGemini305Device(vid, pid)) {
        OBHardwareDecimationConfig decimationConfig{};
        decimationConfig.originWidth  = originWidth;
        decimationConfig.originHeight = originHeight;
        decimationConfig.factor       = presetConfig.depthDecimationFactor;

        if(depthSensor) {
            config->enableVideoStream(OB_SENSOR_DEPTH, decimationConfig);
        }
        if(irLeftSensor) {
            config->enableVideoStream(OB_SENSOR_IR_LEFT, decimationConfig);
        }
        if(irRightSensor) {
            config->enableVideoStream(OB_SENSOR_IR_RIGHT, decimationConfig);
        }
    }
    else {
        if(depthSensor) {
            auto profile = findProfile(depthSensor, originWidth, originHeight, presetConfig.depthDecimationFactor);
            if(profile) {
                config->enableStream(profile);
            }
            else {
                std::cerr << "  Depth    : no matching profile found" << std::endl;
            }
        }
        if(irLeftSensor) {
            auto profile = findProfile(irLeftSensor, originWidth, originHeight, presetConfig.irDecimationFactor);
            if(profile) {
                config->enableStream(profile);
            }
            else {
                std::cerr << "  IR Left  : no matching profile found" << std::endl;
            }
        }
        if(irRightSensor) {
            auto profile = findProfile(irRightSensor, originWidth, originHeight, presetConfig.irDecimationFactor);
            if(profile) {
                config->enableStream(profile);
            }
            else {
                std::cerr << "  IR Right : no matching profile found" << std::endl;
            }
        }
    }

    // Start the pipeline.
    std::cout << "\nStarting pipeline..." << std::endl;
    pipe.start(config);

    // Render frames in an OpenCV window. Press ESC to exit.
    ob_smpl::CVWindow win("Decimation Viewer", 1280, 800, ob_smpl::ARRANGE_GRID);
    std::cout << "Pipeline started. Press ESC to close." << std::endl;

    while(win.run()) {
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet) {
            win.pushFramesToView(frameSet);
        }
    }

    pipe.stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
