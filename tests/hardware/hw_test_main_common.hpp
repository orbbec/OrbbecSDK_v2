// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "common/test_fixtures.hpp"

#include <string>

namespace hw_test_main {

static const char *kDestructiveTestFilter = "TC_CPP_02_Discovery_HW.TC_CPP_02_04_force_ip:"
                                            "TC_CPP_02_Discovery_HW.TC_CPP_02_05_hotplug_reboot:"
                                            "TC_CPP_06_Sensor.TC_CPP_06_09_sensor_after_reboot:"
                                            "TC_CPP_21_Firmware.TC_CPP_21_07_firmware_update:"
                                            "TC_CPP_21_Firmware.TC_CPP_21_08_update_depth_presets";

inline void setDefaultFilterIfUnset(const std::string &filter) {
    auto &currentFilter = ::testing::GTEST_FLAG(filter);
    if(currentFilter.empty() || currentFilter == "*") {
        currentFilter = filter;
    }
}

inline void enableDestructiveMode() {
    if(!ENV().allowDestructive()) {
        return;
    }
    ENV().setAllowDestructive(true);
}

}  // namespace hw_test_main
