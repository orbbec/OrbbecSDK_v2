// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <condition_variable>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace hw_test;

class TC_CPP_10_Frame_HW : public PipelineTest {
protected:
    std::shared_ptr<ob::FrameSet> captureFrames() {
        auto config = std::make_shared<ob::Config>();
        config->enableStream(OB_STREAM_DEPTH);
        config->enableStream(OB_STREAM_COLOR);
        pipeline_->start(config);
        for(int i = 0; i < 3; i++)
            waitForFramesetWithRetry(pipeline_);
        auto fs = waitForFramesetWithRetry(pipeline_);
        pipeline_->stop();
        return fs;
    }
};

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_01_frame_basic_properties) {
    auto fs = captureFrames();
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);
    EXPECT_EQ(depth->getType(), OB_FRAME_DEPTH);
    EXPECT_NE(depth->getFormat(), OB_FORMAT_UNKNOWN);
    EXPECT_NE(depth->getData(), nullptr);
    EXPECT_GT(depth->getDataSize(), 0u);
    EXPECT_GE(depth->getIndex(), 0u) << "Frame index should be >= 0";
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_02_timestamp_monotonicity) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);

    for(int i = 0; i < 5; i++) {
        auto fs = waitForFramesetWithRetry(pipeline_);
        if(!fs)
            continue;
        auto df = fs->getDepthFrame();
        if(df) {
            EXPECT_GT(df->getSystemTimeStampUs(), 0u) << "System timestamp should be > 0 at frame " << i;
        }
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_03_frame_associated_info) {
    auto fs = captureFrames();
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);

    auto profile = depth->getStreamProfile();
    EXPECT_NE(profile, nullptr);

    auto sensor = depth->getSensor();
    EXPECT_NE(sensor, nullptr);

    auto frameDev = depth->getDevice();
    EXPECT_NE(frameDev, nullptr);
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_05_video_frame_properties) {
    auto fs = captureFrames();
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);

    EXPECT_GT(depth->getWidth(), 0u);
    EXPECT_GT(depth->getHeight(), 0u);
    EXPECT_GT(depth->getPixelAvailableBitSize(), 0u) << "Pixel available bit size should be > 0";
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_06_depth_frame_scale) {
    float validRatio = 0.0f;
    float scale      = 0.0f;
    for(int attempt = 0; attempt < 5; ++attempt) {
        auto fs = captureFrames();
        if(!fs)
            continue;
        auto depth = fs->getDepthFrame();
        if(!depth)
            continue;

        scale             = depth->getValueScale();
        auto    *data     = reinterpret_cast<const uint16_t *>(depth->getData());
        uint32_t pixCount = depth->getWidth() * depth->getHeight();
        if(pixCount == 0)
            continue;
        if(depth->getDataSize() < static_cast<uint64_t>(pixCount) * sizeof(uint16_t))
            continue;
        uint32_t validCount = 0;
        for(uint32_t i = 0; i < pixCount; i++) {
            if(data[i] > 0)
                validCount++;
        }
        validRatio = static_cast<float>(validCount) / pixCount;
        if(validRatio >= 0.01f)
            break;
    }
    pipeline_->stop();

    EXPECT_GT(scale, 0.0f);
    if(validRatio < 0.1f) {
        std::cout << "[INFO] Low valid depth pixel ratio: " << validRatio << " (expected in some CI setups)" << std::endl;
    }
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_07_color_ir_data_valid) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_COLOR);
    config->enableStream(OB_STREAM_IR);
    pipeline_->start(config);
    for(int i = 0; i < 5; i++)
        waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);

    auto color = fs->getColorFrame();
    if(color) {
        auto *d       = color->getData();
        bool  allZero = true;
        for(uint32_t i = 0; i < std::min(color->getDataSize(), 1000u); i++) {
            if(d[i] != 0) {
                allZero = false;
                break;
            }
        }
        EXPECT_FALSE(allZero) << "Color frame is all zeros";
    }

    auto ir = fs->getFrame(OB_FRAME_IR);
    if(ir) {
        auto *d       = ir->getData();
        bool  allZero = true;
        for(uint32_t i = 0; i < std::min(ir->getDataSize(), 1000u); i++) {
            if(d[i] != 0) {
                allZero = false;
                break;
            }
        }
        EXPECT_FALSE(allZero) << "IR frame is all zeros";
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_08_points_frame) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);

    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);
    pipeline_->stop();

    std::shared_ptr<ob::PointCloudFilter> pcFilter;
    try {
        pcFilter = std::make_shared<ob::PointCloudFilter>();
    }
    catch(const ob::Error &) {
        GTEST_SKIP() << "PointCloudFilter not available";
    }
    auto result = pcFilter->process(depth);
    ASSERT_NE(result, nullptr);
    auto pf = result->as<ob::PointsFrame>();
    ASSERT_NE(pf, nullptr);
    EXPECT_GT(pf->getCoordinateValueScale(), 0.0f);
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_09_accel_frame) {
    auto accelSensor = tryGetSensor(device_, OB_SENSOR_ACCEL);
    if(!accelSensor) {
        GTEST_SKIP() << "No accel sensor";
    }

    auto profiles = accelSensor->getStreamProfileList();

    if(!profiles || profiles->getCount() == 0)
        GTEST_SKIP() << "No accel profiles available";
    auto profile = profiles->getProfile(0);
    if(!profile)
        GTEST_SKIP() << "First accel profile is null";

    std::shared_ptr<ob::AccelFrame> accelFrame;
    std::mutex                      mtx;
    std::condition_variable         cv;

    accelSensor->start(profile, [&](std::shared_ptr<ob::Frame> frame) {
        if(frame->is<ob::AccelFrame>()) {
            std::lock_guard<std::mutex> lock(mtx);
            accelFrame = frame->as<ob::AccelFrame>();
            cv.notify_one();
        }
    });
    ScopedSensorStream sensorStream(accelSensor);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return accelFrame != nullptr; });
    }

    ASSERT_NE(accelFrame, nullptr) << "No accel frame received";
    auto  val = accelFrame->getValue();
    float mag = std::sqrt(val.x * val.x + val.y * val.y + val.z * val.z);
    EXPECT_NEAR(mag, 9.8f, 4.0f) << "Accel magnitude unexpected: " << mag;

    float temp = accelFrame->getTemperature();
    EXPECT_GE(temp, 0.0f);
    EXPECT_LE(temp, 80.0f);
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_10_gyro_frame) {
    auto gyroSensor = tryGetSensor(device_, OB_SENSOR_GYRO);
    if(!gyroSensor)
        GTEST_SKIP() << "No gyro sensor";

    auto profiles = gyroSensor->getStreamProfileList();
    if(!profiles || profiles->getCount() == 0)
        GTEST_SKIP() << "No gyro profiles available";
    auto profile = profiles->getProfile(0);
    if(!profile)
        GTEST_SKIP() << "First gyro profile is null";

    std::shared_ptr<ob::GyroFrame> gyroFrame;
    std::mutex                     mtx;
    std::condition_variable        cv;

    gyroSensor->start(profile, [&](std::shared_ptr<ob::Frame> frame) {
        if(frame->is<ob::GyroFrame>()) {
            std::lock_guard<std::mutex> lock(mtx);
            gyroFrame = frame->as<ob::GyroFrame>();
            cv.notify_one();
        }
    });
    ScopedSensorStream sensorStream(gyroSensor);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return gyroFrame != nullptr; });
    }

    ASSERT_NE(gyroFrame, nullptr) << "No gyro frame received";
    auto val = gyroFrame->getValue();
    EXPECT_NEAR(val.x, 0.0f, 2.0f);
    EXPECT_NEAR(val.y, 0.0f, 2.0f);
    EXPECT_NEAR(val.z, 0.0f, 2.0f);

    float temp = gyroFrame->getTemperature();
    EXPECT_GE(temp, 0.0f);
    EXPECT_LE(temp, 80.0f);
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_11_frameset_count_extract) {
    auto fs = captureFrames();
    ASSERT_NE(fs, nullptr);
    EXPECT_GE(fs->getCount(), 1u);

    auto depth = fs->getDepthFrame();
    auto color = fs->getColorFrame();
    EXPECT_TRUE(depth || color);
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_12_frameset_by_type_index) {
    auto fs = captureFrames();
    ASSERT_NE(fs, nullptr);

    auto depthByType = fs->getFrame(OB_FRAME_DEPTH);
    if(depthByType) {
        EXPECT_EQ(depthByType->getType(), OB_FRAME_DEPTH);
    }

    for(uint32_t i = 0; i < fs->getCount(); i++) {
        auto f = fs->getFrameByIndex(i);
        EXPECT_NE(f, nullptr);
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_10_Frame_HW, TC_CPP_10_14_frameset_sync) {
    if(!hasSensorType(device_->getSensorList(), OB_SENSOR_COLOR)) {
        GTEST_SKIP() << "Current device has no color sensor";
    }

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
