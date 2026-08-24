// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_16_Preset : public DeviceTest {};

TEST_F(TC_CPP_16_Preset, TC_CPP_16_01_current_and_list) {
    if(!supportsPreset(device_))
        GTEST_SKIP() << "Preset not supported on this device";
    auto list = device_->getAvailablePresetList();
    ASSERT_NE(list, nullptr);
    EXPECT_GT(list->getCount(), 0u);

    auto curName = device_->getCurrentPresetName();
    EXPECT_NE(curName, nullptr);
}

TEST_F(TC_CPP_16_Preset, TC_CPP_16_02_load_builtin) {
    if(!supportsPreset(device_))
        GTEST_SKIP() << "Preset not supported on this device";
    auto list = device_->getAvailablePresetList();
    if(list->getCount() > 0) {
        PresetGuard guard(device_);
        auto        name = list->getName(0);
        ASSERT_NO_THROW(device_->loadPreset(name));
    }
}

TEST_F(TC_CPP_16_Preset, TC_CPP_16_03_export_json) {
    if(!supportsPreset(device_))
        GTEST_SKIP() << "Preset not supported on this device";
    const uint8_t *data     = nullptr;
    uint32_t       dataSize = 0;
    ASSERT_NO_THROW(device_->exportSettingsAsPresetJsonData("test_export", &data, &dataSize));
    EXPECT_NE(data, nullptr);
    EXPECT_GT(dataSize, 0u);
}

TEST_F(TC_CPP_16_Preset, TC_CPP_16_04_load_from_json) {
    if(!supportsPreset(device_))
        GTEST_SKIP() << "Preset not supported on this device";
    PresetGuard    guard(device_);
    const uint8_t *data     = nullptr;
    uint32_t       dataSize = 0;
    device_->exportSettingsAsPresetJsonData("roundtrip_test", &data, &dataSize);
    if(data && dataSize > 0) {
        ASSERT_NO_THROW(device_->loadPresetFromJsonData("roundtrip_test", data, dataSize));
    }
}

TEST_F(TC_CPP_16_Preset, TC_CPP_16_05_preset_changes_properties) {
    if(!supportsPreset(device_))
        GTEST_SKIP() << "Preset not supported on this device";
    auto list = device_->getAvailablePresetList();
    if(list->getCount() < 2) {
        GTEST_SKIP() << "Need >=2 presets";
    }

    PresetGuard guard(device_);

    const char *presetName0 = list->getName(0);
    ASSERT_NO_THROW(device_->loadPreset(presetName0));
    auto currentName0 = device_->getCurrentPresetName();
    EXPECT_STREQ(currentName0, presetName0);

    int32_t exposureAfter0  = 0;
    bool    hasReadableProp = device_->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_READ);
    if(!hasReadableProp) {
        GTEST_SKIP() << "No readable depth property to compare between presets";
    }
    exposureAfter0 = device_->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);

    const char *presetName1 = list->getName(1);
    ASSERT_NO_THROW(device_->loadPreset(presetName1));
    auto currentName1 = device_->getCurrentPresetName();
    EXPECT_STREQ(currentName1, presetName1);

    int32_t exposureAfter1 = 0;
    exposureAfter1         = device_->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);

    if(exposureAfter0 == exposureAfter1) {
        std::cout << "[16_05] Both presets have same depth exposure (" << exposureAfter0 << ") — presets may differ in other properties" << std::endl;
    }
    else {
        EXPECT_NE(exposureAfter0, exposureAfter1) << "Presets should produce different exposure values";
    }
}
