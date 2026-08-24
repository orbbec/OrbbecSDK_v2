// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace hw_test;

class TC_CPP_09_Config : public PipelineTest {};

TEST_F(TC_CPP_09_Config, TC_CPP_09_01_enable_stream_by_type) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    auto fs = waitForFramesetWithRetry(pipeline_);
    EXPECT_NE(fs, nullptr);
    if(fs) {
        EXPECT_NE(fs->getDepthFrame(), nullptr);
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_02_enable_video_stream_params) {
    auto config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_DEPTH, 0, 0, 0, OB_FORMAT_Y16);
    pipeline_->start(config);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    EXPECT_NE(fs->getDepthFrame(), nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_03_enable_imu_stream) {
    if(!tryGetSensor(device_, OB_SENSOR_ACCEL) || !tryGetSensor(device_, OB_SENSOR_GYRO)) {
        GTEST_SKIP() << "Device does not support IMU (accel/gyro)";
    }
    auto config = std::make_shared<ob::Config>();
    config->enableAccelStream();
    config->enableGyroStream();
    pipeline_->start(config);
    auto frameSet = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(frameSet, nullptr) << "IMU: failed to acquire frameset";
    auto accelFrame = frameSet->getFrame(OB_FRAME_ACCEL);
    EXPECT_NE(accelFrame, nullptr);
    auto gyroFrame = frameSet->getFrame(OB_FRAME_GYRO);
    EXPECT_NE(gyroFrame, nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_04_enable_disable_all) {
    auto config = std::make_shared<ob::Config>();

    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr) << "enableStream(DEPTH): failed to acquire frameset";

    auto depthFrame1 = fs->getDepthFrame();
    EXPECT_NE(depthFrame1, nullptr) << "enableStream(DEPTH): expected depth frame";
    pipeline_->stop();

    config->disableAllStream();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);

    auto fs2 = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs2, nullptr) << "disableAll + enableStream(DEPTH): failed to acquire frameset";

    auto depthFrame2 = fs2->getDepthFrame();
    EXPECT_NE(depthFrame2, nullptr) << "disableAll + enableStream(DEPTH): expected depth frame";
    pipeline_->stop();
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_05_enabled_profiles) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    auto profiles = config->getEnabledStreamProfileList();
    EXPECT_GE(profiles->getCount(), 2u);
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_06_d2c_align_mode) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);

    struct AlignCase {
        OBAlignMode mode;
        const char *label;
    };
    const std::vector<AlignCase> cases = {
        { ALIGN_D2C_HW_MODE, "HW" },
        { ALIGN_D2C_SW_MODE, "SW" },
        { ALIGN_DISABLE, "DISABLE" },
    };

    int startedCount = 0;

    for(const auto &c: cases) {
        config->setAlignMode(c.mode);
        bool started = false;
        try {
            pipeline_->start(config);
            started = true;
        }
        catch(const ob::Error &e) {
            std::cout << "[09_06] " << c.label << " align not supported: " << e.what() << std::endl;
            continue;
        }

        if(started) {
            startedCount++;
            auto fs = waitForFramesetWithRetry(pipeline_);
            ASSERT_NE(fs, nullptr) << c.label << " align mode: no frames received";
            pipeline_->stop();
        }
    }

    if(startedCount == 0) {
        GTEST_SKIP() << "No D2C align mode supported on this device";
    }
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_07_depth_scale_after_align) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    config->setAlignMode(ALIGN_D2C_SW_MODE);
    config->setDepthScaleRequire(true);
    try {
        pipeline_->start(config);
    }
    catch(const ob::Error &e) {
        GTEST_SKIP() << "SW align not supported: " << e.what();
    }
    auto fs = waitForFramesetWithRetry(pipeline_);
    EXPECT_NE(fs, nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_09_Config, TC_CPP_09_08_frame_aggregate_mode) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);

    auto testAggregateMode = [&](OBFrameAggregateOutputMode mode) {
        config->setFrameAggregateOutputMode(mode);
        pipeline_->start(config);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        auto fs = waitForFramesetWithRetry(pipeline_, 30);
        pipeline_->stop();
        return fs;
    };

    auto fs1 = testAggregateMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
    EXPECT_NE(fs1, nullptr) << "ANY_SITUATION mode failed";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto fs2 = testAggregateMode(OB_FRAME_AGGREGATE_OUTPUT_COLOR_FRAME_REQUIRE);
    EXPECT_NE(fs2, nullptr) << "COLOR_FRAME_REQUIRE mode failed";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto fs3 = testAggregateMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    EXPECT_NE(fs3, nullptr) << "ALL_TYPE_FRAME_REQUIRE mode failed";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto fs4 = testAggregateMode(OB_FRAME_AGGREGATE_OUTPUT_DISABLE);
    EXPECT_NE(fs4, nullptr) << "DISABLE mode failed";
}
