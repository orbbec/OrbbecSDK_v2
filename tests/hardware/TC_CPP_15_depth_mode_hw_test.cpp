// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_15_DepthMode : public DeviceTest {};

TEST_F(TC_CPP_15_DepthMode, TC_CPP_15_01_current_mode) {
    if(!supportsDepthWorkMode(device_))
        GTEST_SKIP() << "Depth work mode not supported on this device";
    auto mode = device_->getCurrentDepthWorkMode();
    EXPECT_GT(std::strlen(mode.name), 0u);
    auto name = device_->getCurrentDepthModeName();
    EXPECT_NE(name, nullptr);
}

TEST_F(TC_CPP_15_DepthMode, TC_CPP_15_02_enum_modes) {
    if(!supportsDepthWorkMode(device_))
        GTEST_SKIP() << "Depth work mode not supported on this device";
    auto list = device_->getDepthWorkModeList();
    ASSERT_NE(list, nullptr);
    EXPECT_GT(list->getCount(), 0u);

    for(uint32_t i = 0; i < list->getCount(); i++) {
        auto mode = list->getOBDepthWorkMode(i);
        EXPECT_GT(std::strlen(mode.name), 0u);
    }
}

TEST_F(TC_CPP_15_DepthMode, TC_CPP_15_03_switch_by_name) {
    if(!supportsDepthWorkMode(device_))
        GTEST_SKIP() << "Depth work mode not supported on this device";
    auto list = device_->getDepthWorkModeList();
    ASSERT_GT(list->getCount(), 0u);

    auto           originalMode = device_->getCurrentDepthWorkMode();
    std::string    originalName(originalMode.name);
    DepthModeGuard guard(device_);

    auto targetMode = list->getOBDepthWorkMode(0);
    if(std::string(targetMode.name) == originalName && list->getCount() > 1) {
        targetMode = list->getOBDepthWorkMode(1);
    }
    auto status = device_->switchDepthWorkMode(targetMode.name);
    EXPECT_EQ(status, OB_STATUS_OK);
}

TEST_F(TC_CPP_15_DepthMode, TC_CPP_15_04_switch_by_struct) {
    if(!supportsDepthWorkMode(device_))
        GTEST_SKIP() << "Depth work mode not supported on this device";
    auto list = device_->getDepthWorkModeList();
    ASSERT_GT(list->getCount(), 0u);
    DepthModeGuard guard(device_);
    auto           mode = list->getOBDepthWorkMode(0);

    auto status = device_->switchDepthWorkMode(mode);
    EXPECT_EQ(status, OB_STATUS_OK);
}

TEST_F(TC_CPP_15_DepthMode, TC_CPP_15_05_profile_change_after_switch) {
    if(!supportsDepthWorkMode(device_))
        GTEST_SKIP() << "Depth work mode not supported on this device";
    auto list = device_->getDepthWorkModeList();
    if(list->getCount() < 2)
        GTEST_SKIP() << "Only 1 depth mode available";

    auto           originalMode = device_->getCurrentDepthWorkMode();
    DepthModeGuard guard(device_);

    auto depthSensor    = tryGetSensor(device_, OB_SENSOR_DEPTH);
    auto profilesBefore = depthSensor->getStreamProfileList();
    ASSERT_GT(profilesBefore->getCount(), 0u);

    auto targetMode = list->getOBDepthWorkMode(1);
    ASSERT_NO_THROW(device_->switchDepthWorkMode(targetMode.name));

    auto switchedMode = device_->getCurrentDepthWorkMode();
    EXPECT_STREQ(switchedMode.name, targetMode.name) << "Depth work mode did not switch as requested";

    auto profilesAfter = depthSensor->getStreamProfileList();
    EXPECT_GT(profilesAfter->getCount(), 0u);

    auto countBefore = profilesBefore->getCount();
    auto countAfter  = profilesAfter->getCount();

    bool profileChanged = (countBefore != countAfter);
    if(!profileChanged) {
        for(uint32_t i = 0; i < countBefore && i < countAfter; i++) {
            auto before = profilesBefore->getProfile(i)->as<ob::VideoStreamProfile>();
            auto after  = profilesAfter->getProfile(i)->as<ob::VideoStreamProfile>();
            if(before->getWidth() != after->getWidth() || before->getHeight() != after->getHeight()) {
                profileChanged = true;
                break;
            }
        }
    }

    if(!profileChanged) {
        std::cout << "[15_05] Profile list unchanged after mode switch — modes may share identical profiles" << std::endl;
    }
}
