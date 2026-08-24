// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace hw_test;

class TC_CPP_07_StreamProfile : public SensorTest {};

TEST_F(TC_CPP_07_StreamProfile, TC_CPP_07_01_depth_color_profiles) {
    for(auto sensorType: { OB_SENSOR_DEPTH, OB_SENSOR_COLOR }) {
        auto sensor = tryGetSensor(device_, sensorType);
        if(!sensor)
            continue;  // optional sensor not present on this device
        auto profiles = sensor->getStreamProfileList();
        ASSERT_GT(profiles->getCount(), 0u) << "No profiles for sensor type " << sensorType;

        for(uint32_t i = 0; i < profiles->getCount(); i++) {
            auto p = profiles->getProfile(i)->as<ob::VideoStreamProfile>();
            EXPECT_GT(p->getWidth(), 0u);
            EXPECT_GT(p->getHeight(), 0u);
            EXPECT_GT(p->getFps(), 0u);
            EXPECT_NE(p->getFormat(), OB_FORMAT_UNKNOWN) << "Profile format should be valid";
        }
    }
}

TEST_F(TC_CPP_07_StreamProfile, TC_CPP_07_02_video_profile_filter) {
    auto pipeline      = std::make_shared<ob::Pipeline>(device_);
    auto depthProfiles = pipeline->getStreamProfileList(OB_SENSOR_DEPTH);
    ASSERT_GT(depthProfiles->getCount(), 0u);

    auto first = depthProfiles->getProfile(0)->as<ob::VideoStreamProfile>();
    auto w     = first->getWidth();
    auto h     = first->getHeight();
    auto fps   = first->getFps();

    std::shared_ptr<ob::VideoStreamProfile> matched;
    ASSERT_NO_THROW({ matched = depthProfiles->getVideoStreamProfile(w, h, first->getFormat(), fps); });
    ASSERT_NE(matched, nullptr) << "getVideoStreamProfile failed to find the first profile";
    EXPECT_EQ(matched->getWidth(), w);
    EXPECT_EQ(matched->getHeight(), h);
    EXPECT_EQ(matched->getFormat(), first->getFormat());
    EXPECT_EQ(matched->getFps(), fps);

    try {
        auto invalid = depthProfiles->getVideoStreamProfile(99999, 99999, OB_FORMAT_Y16, 999);
        EXPECT_EQ(invalid, nullptr) << "Invalid profile parameters should return null";
    }
    catch(const ob::Error &) {
    }
}

TEST_F(TC_CPP_07_StreamProfile, TC_CPP_07_03_accel_profile) {
    auto accel = tryGetSensor(device_, OB_SENSOR_ACCEL);
    if(!accel)
        GTEST_SKIP() << "No accel sensor";

    auto profiles = accel->getStreamProfileList();
    ASSERT_GT(profiles->getCount(), 0u);

    auto ap = profiles->getProfile(0)->as<ob::AccelStreamProfile>();
    EXPECT_NE(ap->getFullScaleRange(), 0);
    EXPECT_NE(ap->getSampleRate(), 0);
}

TEST_F(TC_CPP_07_StreamProfile, TC_CPP_07_04_gyro_profile) {
    auto gyro = tryGetSensor(device_, OB_SENSOR_GYRO);
    if(!gyro)
        GTEST_SKIP() << "No gyro sensor";

    auto profiles = gyro->getStreamProfileList();
    ASSERT_GT(profiles->getCount(), 0u);

    auto gp = profiles->getProfile(0)->as<ob::GyroStreamProfile>();
    EXPECT_NE(gp->getFullScaleRange(), 0);
    EXPECT_NE(gp->getSampleRate(), 0);
}

TEST_F(TC_CPP_07_StreamProfile, TC_CPP_07_05_profile_type_check) {
    auto depthSensor = tryGetSensor(device_, OB_SENSOR_DEPTH);
    auto profiles    = depthSensor->getStreamProfileList();
    auto p           = profiles->getProfile(0);

    EXPECT_TRUE(p->is<ob::VideoStreamProfile>());
    auto vp = p->as<ob::VideoStreamProfile>();
    ASSERT_NE(vp, nullptr);

    EXPECT_FALSE(p->is<ob::AccelStreamProfile>());
    EXPECT_THROW(p->as<ob::AccelStreamProfile>(), std::runtime_error);

    auto accelSensor = tryGetSensor(device_, OB_SENSOR_ACCEL);
    if(accelSensor) {
        auto accelProfiles = accelSensor->getStreamProfileList();
        auto ap            = accelProfiles->getProfile(0);

        EXPECT_TRUE(ap->is<ob::AccelStreamProfile>());
        auto asp = ap->as<ob::AccelStreamProfile>();
        ASSERT_NE(asp, nullptr);
    }

    auto gyroSensor = tryGetSensor(device_, OB_SENSOR_GYRO);
    if(gyroSensor) {
        auto gyroProfiles = gyroSensor->getStreamProfileList();
        auto gp           = gyroProfiles->getProfile(0);

        EXPECT_TRUE(gp->is<ob::GyroStreamProfile>());
        auto gsp = gp->as<ob::GyroStreamProfile>();
        ASSERT_NE(gsp, nullptr);
    }
}
