// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "utils.hpp"
#include "utils_c.h"

#include <chrono>

namespace ob_smpl {
char waitForKeyPressed(uint32_t timeout_ms) {
    return ob_smpl_wait_for_key_press(timeout_ms);
}

uint64_t getNowTimesMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int getInputOption() {
    char inputOption = ob_smpl::waitForKeyPressed();
    if(inputOption == ESC_KEY) {
        return -1;
    }
    return inputOption - '0';
}

bool isLiDARDevice(std::shared_ptr<ob::Device> device) {
    std::shared_ptr<ob::SensorList> sensorList = device->getSensorList();

    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        OBSensorType sensorType = sensorList->getSensorType(index);
        if(sensorType == OB_SENSOR_LIDAR) {
            return true;
        }
    }

    return false;
}

bool supportAnsiEscape() {
    if(ob_smpl_support_ansi_escape() == 0) {
        return false;
    }
    return true;
}

bool isGemini305Device(int vid, int pid) {
    return ob_smpl_is_gemini305_device(vid, pid);
}

bool isAstraMiniDevice(int vid, int pid) {
    return ob_smpl_is_astra_mini_device(vid, pid);
}

}  // namespace ob_smpl
