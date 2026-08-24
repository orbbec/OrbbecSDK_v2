// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <atomic>
#include <cstring>
#include <string>
#include <thread>

using namespace hw_test;

class TC_CPP_08_Pipeline : public PipelineTest {};

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_01_pipeline_construct) {
    ASSERT_NE(pipeline_, nullptr);

    auto pipeDev = pipeline_->getDevice();
    ASSERT_NE(pipeDev, nullptr);
    auto pipeInfo = pipeDev->getDeviceInfo();
    ASSERT_NE(pipeInfo, nullptr);
    EXPECT_STREQ(pipeInfo->getSerialNumber(), devInfo_->getSerialNumber());

    auto depthProfiles = pipeline_->getStreamProfileList(OB_SENSOR_DEPTH);
    ASSERT_NE(depthProfiles, nullptr);
    EXPECT_GT(depthProfiles->getCount(), 0u);
    for(uint32_t i = 0; i < depthProfiles->getCount(); ++i) {
        auto profile = depthProfiles->getProfile(i);
        ASSERT_NE(profile, nullptr);
        auto videoProfile = profile->as<ob::VideoStreamProfile>();
        ASSERT_NE(videoProfile, nullptr);
        EXPECT_GT(videoProfile->getWidth(), 0u);
        EXPECT_GT(videoProfile->getHeight(), 0u);
    }
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_02_default_start_stop) {
    pipeline_->start();
    auto frames = waitForFramesetWithRetry(pipeline_);
    EXPECT_NE(frames, nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_03_config_start_poll) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    pipeline_->start(config);

    auto fs = waitForFramesetWithRetry(pipeline_);
    pipeline_->stop();
    EXPECT_NE(fs, nullptr) << "No frameset received";
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_04_config_start_callback) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    if(hasSensorType(device_->getSensorList(), OB_SENSOR_COLOR)) {
        config->enableStream(OB_STREAM_COLOR);
    }

    std::atomic<int> cbCount{ 0 };
    pipeline_->start(config, [&cbCount](std::shared_ptr<ob::FrameSet>) { cbCount++; });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    pipeline_->stop();

    EXPECT_GT(cbCount.load(), 0) << "No callback frames received";
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_05_switch_config) {
    auto config1 = std::make_shared<ob::Config>();
    config1->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config1);
    waitForFramesetWithRetry(pipeline_);

    auto config2 = std::make_shared<ob::Config>();
    config2->enableStream(OB_STREAM_DEPTH);
    config2->enableStream(OB_STREAM_COLOR);

    ASSERT_NO_THROW(pipeline_->stop());
    ASSERT_NO_THROW(pipeline_->start(config2));
    auto fs = waitForFramesetWithRetry(pipeline_);
    EXPECT_NE(fs, nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_06_get_bound_device) {
    auto pipeDev = pipeline_->getDevice();
    ASSERT_NE(pipeDev, nullptr);
    auto pipeInfo = pipeDev->getDeviceInfo();
    EXPECT_STREQ(pipeInfo->getSerialNumber(), devInfo_->getSerialNumber());
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_07_get_stream_profile_list) {
    auto depthProfiles = pipeline_->getStreamProfileList(OB_SENSOR_DEPTH);
    ASSERT_NE(depthProfiles, nullptr);
    EXPECT_GT(depthProfiles->getCount(), 0u);
    for(uint32_t i = 0; i < depthProfiles->getCount(); ++i) {
        auto profile = depthProfiles->getProfile(i);
        ASSERT_NE(profile, nullptr);
        auto videoProfile = profile->as<ob::VideoStreamProfile>();
        ASSERT_NE(videoProfile, nullptr);
        EXPECT_GT(videoProfile->getWidth(), 0u);
        EXPECT_GT(videoProfile->getHeight(), 0u);
    }

    auto colorProfiles = pipeline_->getStreamProfileList(OB_SENSOR_COLOR);
    if(colorProfiles) {
        EXPECT_GT(colorProfiles->getCount(), 0u);
    }
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_08_frame_sync) {
    pipeline_->enableFrameSync();
    ScopeGuard disableSync([&] { pipeline_->disableFrameSync(); });

    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    pipeline_->start(config);
    auto fs = waitForFramesetWithRetry(pipeline_);
    pipeline_->stop();

    EXPECT_NE(fs, nullptr) << "No frameset received";
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_09_d2c_depth_profile_list) {
    auto colorProfiles = pipeline_->getStreamProfileList(OB_SENSOR_COLOR);
    if(!colorProfiles || colorProfiles->getCount() == 0) {
        GTEST_SKIP() << "No color profiles available";
    }

    bool foundSupported = false;

    for(uint32_t i = 0; i < colorProfiles->getCount(); ++i) {
        auto colorProfile = colorProfiles->getProfile(i);
        try {
            auto d2cList = pipeline_->getD2CDepthProfileList(colorProfile, ALIGN_D2C_HW_MODE);
            if(d2cList && d2cList->getCount() > 0) {
                foundSupported = true;
                break;
            }
        }
        catch(const ob::Error &) {
        }
    }

    if(!foundSupported) {
        GTEST_SKIP() << "Current device exposes no color profile with a non-empty HW D2C depth profile list";
    }

    foundSupported = false;
    for(uint32_t i = 0; i < colorProfiles->getCount(); ++i) {
        auto colorProfile = colorProfiles->getProfile(i);
        try {
            auto d2cList = pipeline_->getD2CDepthProfileList(colorProfile, ALIGN_D2C_SW_MODE);
            if(d2cList && d2cList->getCount() > 0) {
                foundSupported = true;
                break;
            }
        }
        catch(const ob::Error &) {
        }
    }

    if(!foundSupported) {
        GTEST_SKIP() << "Current device exposes no color profile with a non-empty SW D2C depth profile list";
    }
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_10_camera_param) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    pipeline_->start(config);
    waitForFramesetWithRetry(pipeline_);

    auto param = pipeline_->getCameraParam();
    EXPECT_GT(param.depthIntrinsic.fx, 0.0f);
    EXPECT_GT(param.depthIntrinsic.fy, 0.0f);
    EXPECT_GT(param.depthIntrinsic.cx, 0.0f);
    EXPECT_GT(param.depthIntrinsic.cy, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.fx, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.fy, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.cx, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.cy, 0.0f);
    pipeline_->stop();
}

TEST_F(TC_CPP_08_Pipeline, TC_CPP_08_11_calibration_param) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);

    auto calibParam = pipeline_->getCalibrationParam(config);
    EXPECT_GT(calibParam.intrinsics[OB_SENSOR_DEPTH].fx, 0.0f);
    EXPECT_GT(calibParam.intrinsics[OB_SENSOR_COLOR].fx, 0.0f);
}
