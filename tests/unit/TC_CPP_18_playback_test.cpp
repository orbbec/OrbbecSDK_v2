// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

using test_utils::getFirstVideoProfile;
using test_utils::hasSensorType;
using test_utils::waitForFramesetWithRetry;

namespace {

template <typename T> static void expectRangeValid(const T &range) {
    EXPECT_LE(range.min, range.max);
}

}  // namespace

class TC_CPP_18_Playback : public SDKTestBase {
protected:
    void SetUp() override {
        SDKTestBase::SetUp();
        const auto &bag = cfg_.bagPath();
        if(bag.empty() || !test_utils::fileExists(bag)) {
            GTEST_SKIP() << "Playback bag not available ('" << bag << "') — skipping playback test";
        }
    }
};

TEST_F(TC_CPP_18_Playback, TC_CPP_18_03_playback_device_create_and_play) {
    auto bagPath  = cfg_.bagPath();
    auto pbDevice = std::make_shared<ob::PlaybackDevice>(bagPath);
    ASSERT_NE(pbDevice, nullptr);

    auto devInfo = pbDevice->getDeviceInfo();
    ASSERT_NE(devInfo, nullptr);
    EXPECT_NE(devInfo->getName(), nullptr);

    auto pipeline   = std::make_shared<ob::Pipeline>(pbDevice);
    auto config     = std::make_shared<ob::Config>();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    ASSERT_GT(sensorList->getCount(), 0u) << "No sensors found in playback file";
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        config->enableStream(sensorList->getSensorType(i));
    }
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
    pipeline->start(config);

    auto frames = waitForFramesetWithRetry(pipeline);
    EXPECT_NE(frames, nullptr) << "No frames from playback";

    pipeline->stop();
}

TEST_F(TC_CPP_18_Playback, TC_CPP_18_04_playback_duration_position) {
    auto pbDevice = std::make_shared<ob::PlaybackDevice>(cfg_.bagPath());
    ASSERT_NE(pbDevice, nullptr);

    auto duration = pbDevice->getDuration();
    EXPECT_GT(duration, 0u) << "Duration should be > 0";

    auto position = pbDevice->getPosition();
    EXPECT_GE(position, 0u);
}

TEST_F(TC_CPP_18_Playback, TC_CPP_18_05_playback_seek_and_rate) {
    auto pbDevice = std::make_shared<ob::PlaybackDevice>(cfg_.bagPath());
    ASSERT_NE(pbDevice, nullptr);

    auto duration = pbDevice->getDuration();
    if(duration > 100) {
        ASSERT_NO_THROW(pbDevice->seek(duration / 2));
    }

    ASSERT_NO_THROW(pbDevice->setPlaybackRate(2.0f));
    ASSERT_NO_THROW(pbDevice->setPlaybackRate(0.5f));
    ASSERT_NO_THROW(pbDevice->setPlaybackRate(1.0f));
}

TEST_F(TC_CPP_18_Playback, TC_CPP_18_06_playback_pause_resume_status) {
    auto pbDevice = std::make_shared<ob::PlaybackDevice>(cfg_.bagPath());
    ASSERT_NE(pbDevice, nullptr);

    auto pipeline   = std::make_shared<ob::Pipeline>(pbDevice);
    auto config     = std::make_shared<ob::Config>();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    ASSERT_GT(sensorList->getCount(), 0u) << "No sensors found in playback file";
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        config->enableStream(sensorList->getSensorType(i));
    }
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
    pipeline->start(config);

    waitForFramesetWithRetry(pipeline);

    ASSERT_NO_THROW(pbDevice->pause());
    auto status = pbDevice->getPlaybackStatus();
    EXPECT_EQ(status, OB_PLAYBACK_PAUSED);

    ASSERT_NO_THROW(pbDevice->resume());
    status = pbDevice->getPlaybackStatus();
    EXPECT_EQ(status, OB_PLAYBACK_PLAYING);

    pipeline->stop();
}

TEST_F(TC_CPP_18_Playback, TC_CPP_18_07_playback_status_callback) {
    auto pbDevice = std::make_shared<ob::PlaybackDevice>(cfg_.bagPath());
    ASSERT_NE(pbDevice, nullptr);

    auto cbCount = std::make_shared<std::atomic<int>>(0);
    pbDevice->setPlaybackStatusChangeCallback([cbCount](OBPlaybackStatus) { (*cbCount)++; });

    auto pipeline   = std::make_shared<ob::Pipeline>(pbDevice);
    auto config     = std::make_shared<ob::Config>();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    ASSERT_GT(sensorList->getCount(), 0u) << "No sensors found in playback file";
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        config->enableStream(sensorList->getSensorType(i));
    }
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
    pipeline->start(config);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    pbDevice->pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    pbDevice->resume();
    const auto kStart = std::chrono::steady_clock::now();
    while(cbCount->load() < 1) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - kStart).count();
        ASSERT_LT(static_cast<long>(elapsed), 5000) << "Timeout waiting for status callback after resume";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    pipeline->stop();

    EXPECT_GE(cbCount->load(), 1) << "No status change callback received";
}

class TC_CPP_18_PlaybackParam : public SDKTestBase, public ::testing::WithParamInterface<std::string> {
protected:
    void SetUp() override {
        SDKTestBase::SetUp();
        if(GetParam().empty()) {
            GTEST_SKIP() << "No playback bag path provided for parameterized test";
        }
    }

    std::shared_ptr<ob::PlaybackDevice> createPlaybackDevice() const {
        return std::make_shared<ob::PlaybackDevice>(GetParam());
    }

    std::shared_ptr<ob::Pipeline> createStartedPipeline(const std::shared_ptr<ob::PlaybackDevice> &pbDevice) const {
        auto pipeline   = std::make_shared<ob::Pipeline>(pbDevice);
        auto config     = std::make_shared<ob::Config>();
        auto sensorList = pbDevice->getSensorList();
        EXPECT_NE(sensorList, nullptr);
        if(!sensorList || sensorList->getCount() == 0) {
            return nullptr;
        }
        for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
            config->enableStream(sensorList->getSensorType(i));
        }
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
        pipeline->start(config);
        return pipeline;
    }

    std::shared_ptr<ob::DepthFrame> getDepthFrameFromPlayback(const std::shared_ptr<ob::PlaybackDevice> &pbDevice) const {
        auto pipeline = createStartedPipeline(pbDevice);
        if(!pipeline) {
            return nullptr;
        }
        std::shared_ptr<ob::DepthFrame> depth;
        for(int i = 0; i < 5 && !depth; ++i) {
            auto frames = waitForFramesetWithRetry(pipeline);
            if(!frames)
                continue;
            depth = frames->getDepthFrame();
        }
        pipeline->stop();
        return depth;
    }

    template <typename TFilter, typename Fn> void runPrivateFilterCase(const Fn &fn) {
        try {
            auto filter = std::make_shared<TFilter>("");
            ASSERT_NE(filter, nullptr);
            fn(filter);
        }
        catch(const ob::Error &e) {
            std::string msg = e.what() ? e.what() : "";
            if(msg.find("Private filter") != std::string::npos || msg.find("activated") != std::string::npos) {
                GTEST_SKIP() << "Private filter not activated: " << msg;
            }
            throw;
        }
    }
};

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_10_playback_device_info) {
    auto pbDevice = createPlaybackDevice();
    ASSERT_NE(pbDevice, nullptr);

    auto info = pbDevice->getDeviceInfo();
    ASSERT_NE(info, nullptr);
    ASSERT_NE(info->getName(), nullptr);
    EXPECT_GT(std::strlen(info->getName()), 0u);
    EXPECT_NE(info->getSerialNumber(), nullptr);
    EXPECT_NE(info->getFirmwareVersion(), nullptr);
    EXPECT_NE(info->getConnectionType(), nullptr);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_11_playback_sensor_list) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    ASSERT_GT(sensorList->getCount(), 0u);

    EXPECT_TRUE(hasSensorType(sensorList, OB_SENSOR_DEPTH));
    EXPECT_TRUE(hasSensorType(sensorList, OB_SENSOR_COLOR));
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_12_playback_stream_profile_list) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    ASSERT_GT(sensorList->getCount(), 0u);

    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        auto sensor = sensorList->getSensor(i);
        ASSERT_NE(sensor, nullptr);

        auto profileList = sensor->getStreamProfileList();
        ASSERT_NE(profileList, nullptr);
        ASSERT_GT(profileList->getCount(), 0u);

        for(uint32_t j = 0; j < profileList->getCount(); ++j) {
            auto profile = profileList->getProfile(j);
            ASSERT_NE(profile, nullptr);
            if(profile->is<ob::VideoStreamProfile>()) {
                auto video = profile->as<ob::VideoStreamProfile>();
                EXPECT_GT(video->getWidth(), 0u);
                EXPECT_GT(video->getHeight(), 0u);
                EXPECT_GT(video->getFps(), 0u);
            }
        }
    }
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_20_playback_depth_frame) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    std::shared_ptr<ob::FrameSet> frames;
    for(int i = 0; i < 5 && (!frames || !frames->getDepthFrame()); ++i) {
        frames = waitForFramesetWithRetry(pipeline);
    }
    pipeline->stop();
    ASSERT_NE(frames, nullptr);

    auto depth = frames->getDepthFrame();
    if(!depth) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }
    EXPECT_EQ(depth->getType(), OB_FRAME_DEPTH);
    EXPECT_GT(depth->getWidth(), 0u);
    EXPECT_GT(depth->getHeight(), 0u);
    EXPECT_NE(depth->getData(), nullptr);
    EXPECT_GT(depth->getDataSize(), 0u);
    EXPECT_GT(depth->getTimeStampUs(), 0u);

    uint64_t pixelCount = static_cast<uint64_t>(depth->getWidth()) * depth->getHeight();
    ASSERT_GT(pixelCount, 0u);
    EXPECT_EQ(depth->getDataSize() % pixelCount, 0u);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_21_playback_color_frame) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    std::shared_ptr<ob::FrameSet> frames;
    for(int i = 0; i < 5 && (!frames || !frames->getColorFrame()); ++i) {
        frames = waitForFramesetWithRetry(pipeline);
    }
    pipeline->stop();
    ASSERT_NE(frames, nullptr);

    auto color = frames->getColorFrame();
    if(!color) {
        GTEST_SKIP() << "No color frame in playback bag";
    }

    EXPECT_EQ(color->getType(), OB_FRAME_COLOR);
    EXPECT_GT(color->getWidth(), 0u);
    EXPECT_GT(color->getHeight(), 0u);
    EXPECT_NE(color->getData(), nullptr);
    EXPECT_GT(color->getDataSize(), 0u);

    const std::set<OBFormat> knownColorFormats = {
        OB_FORMAT_RGB,  OB_FORMAT_BGR,  OB_FORMAT_RGBA, OB_FORMAT_BGRA, OB_FORMAT_YUYV,
        OB_FORMAT_UYVY, OB_FORMAT_MJPG, OB_FORMAT_NV12, OB_FORMAT_NV21, OB_FORMAT_I420,
    };
    EXPECT_NE(knownColorFormats.count(color->getFormat()), 0u) << "Unexpected color frame format";
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_22_playback_ir_frame) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    std::shared_ptr<ob::FrameSet> frames;
    for(int i = 0; i < 5; ++i) {
        frames = waitForFramesetWithRetry(pipeline);
        if(!frames)
            continue;
        if(frames->getFrame(OB_FRAME_IR) || frames->getFrame(OB_FRAME_IR_LEFT) || frames->getFrame(OB_FRAME_IR_RIGHT))
            break;
    }
    pipeline->stop();
    ASSERT_NE(frames, nullptr);

    auto ir      = frames->getFrame(OB_FRAME_IR);
    auto irLeft  = frames->getFrame(OB_FRAME_IR_LEFT);
    auto irRight = frames->getFrame(OB_FRAME_IR_RIGHT);
    if(!ir && !irLeft && !irRight) {
        GTEST_SKIP() << "No IR/IR_LEFT/IR_RIGHT frame in playback bag";
    }

    auto validateIr = [](const std::shared_ptr<ob::Frame> &f) {
        if(!f) {
            return;
        }
        EXPECT_NE(f->getData(), nullptr);
        EXPECT_GT(f->getDataSize(), 0u);
    };

    validateIr(ir);
    validateIr(irLeft);
    validateIr(irRight);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_23_playback_frame_metadata) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    std::shared_ptr<ob::FrameSet> frames;
    for(int i = 0; i < 5; ++i) {
        frames = waitForFramesetWithRetry(pipeline);
        if(!frames)
            continue;
        if(frames->getDepthFrame() || frames->getColorFrame())
            break;
    }
    pipeline->stop();
    ASSERT_NE(frames, nullptr);

    std::shared_ptr<ob::Frame> frame = frames->getDepthFrame();
    if(!frame) {
        frame = frames->getColorFrame();
    }
    if(!frame) {
        GTEST_SKIP() << "No depth/color frame for metadata test";
    }

    EXPECT_TRUE(frame->hasMetadata(OB_FRAME_METADATA_TYPE_TIMESTAMP));
    if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_TIMESTAMP)) {
        EXPECT_GT(frame->getMetadataValue(OB_FRAME_METADATA_TYPE_TIMESTAMP), 0);
    }

    int supportedCount = 0;
    for(int i = 0; i < static_cast<int>(OB_FRAME_METADATA_TYPE_COUNT); ++i) {
        auto type = static_cast<OBFrameMetadataType>(i);
        if(frame->hasMetadata(type)) {
            (void)frame->getMetadataValue(type);
            ++supportedCount;
        }
    }
    EXPECT_GT(supportedCount, 0);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_24_playback_frame_timestamps) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    std::shared_ptr<ob::DepthFrame> d1;
    std::shared_ptr<ob::DepthFrame> d2;
    for(int i = 0; i < 5 && (!d1 || !d2); ++i) {
        auto frames = waitForFramesetWithRetry(pipeline);
        if(!frames) {
            break;
        }

        auto depth = frames->getDepthFrame();
        if(!depth) {
            continue;
        }

        if(!d1) {
            d1 = depth;
        }
        else {
            d2 = depth;
        }
    }
    pipeline->stop();

    if(!d1 || !d2) {
        GTEST_SKIP() << "No two depth frames for timestamp monotonic check after retries";
    }

    EXPECT_GT(d1->getTimeStampUs(), 0u);
    EXPECT_GT(d2->getTimeStampUs(), 0u);
    EXPECT_GE(d2->getTimeStampUs(), d1->getTimeStampUs());
    EXPECT_GE(d1->getSystemTimeStampUs(), 0u);
    EXPECT_GE(d1->getGlobalTimeStampUs(), 0u);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_25_playback_frameset_multi_stream) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    auto frames = waitForFramesetWithRetry(pipeline);
    pipeline->stop();
    ASSERT_NE(frames, nullptr);
    ASSERT_GE(frames->getCount(), 2u);

    std::set<int> typeSet;
    for(uint32_t i = 0; i < frames->getCount(); ++i) {
        auto frame = frames->getFrameByIndex(i);
        ASSERT_NE(frame, nullptr);
        int t = static_cast<int>(frame->getType());
        EXPECT_TRUE(typeSet.insert(t).second) << "Duplicate frame type in frameset: " << t;
    }
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_30_playback_depth_intrinsic) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    auto profile    = getFirstVideoProfile(sensorList, OB_SENSOR_DEPTH);
    if(!profile) {
        GTEST_SKIP() << "No depth video profile";
    }

    auto in = profile->getIntrinsic();
    EXPECT_GT(in.fx, 0.0f);
    EXPECT_GT(in.fy, 0.0f);
    EXPECT_GT(in.width, 0);
    EXPECT_GT(in.height, 0);
    EXPECT_GT(in.cx, 0.0f);
    EXPECT_GT(in.cy, 0.0f);
    EXPECT_LT(in.cx, static_cast<float>(in.width));
    EXPECT_LT(in.cy, static_cast<float>(in.height));
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_31_playback_color_intrinsic) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    auto profile    = getFirstVideoProfile(sensorList, OB_SENSOR_COLOR);
    if(!profile) {
        GTEST_SKIP() << "No color video profile";
    }

    auto in = profile->getIntrinsic();
    EXPECT_GT(in.fx, 0.0f);
    EXPECT_GT(in.fy, 0.0f);
    EXPECT_GT(in.width, 0);
    EXPECT_GT(in.height, 0);
    EXPECT_GT(in.cx, 0.0f);
    EXPECT_GT(in.cy, 0.0f);
    EXPECT_LT(in.cx, static_cast<float>(in.width));
    EXPECT_LT(in.cy, static_cast<float>(in.height));
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_32_playback_depth_distortion) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    auto profile    = getFirstVideoProfile(sensorList, OB_SENSOR_DEPTH);
    if(!profile) {
        GTEST_SKIP() << "No depth video profile";
    }

    auto d = profile->getDistortion();
    EXPECT_GE(static_cast<int>(d.model), static_cast<int>(OB_DISTORTION_NONE));
    EXPECT_LE(static_cast<int>(d.model), static_cast<int>(OB_DISTORTION_KANNALA_BRANDT4));
    EXPECT_TRUE(std::isfinite(d.k1));
    EXPECT_TRUE(std::isfinite(d.k2));
    EXPECT_TRUE(std::isfinite(d.k3));
    EXPECT_TRUE(std::isfinite(d.k4));
    EXPECT_TRUE(std::isfinite(d.k5));
    EXPECT_TRUE(std::isfinite(d.k6));
    EXPECT_TRUE(std::isfinite(d.p1));
    EXPECT_TRUE(std::isfinite(d.p2));
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_33_playback_depth_to_color_extrinsic) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    auto depthPrf   = getFirstVideoProfile(sensorList, OB_SENSOR_DEPTH);
    auto colorPrf   = getFirstVideoProfile(sensorList, OB_SENSOR_COLOR);
    if(!depthPrf || !colorPrf) {
        GTEST_SKIP() << "Depth or color profile missing for extrinsic test";
    }

    auto ex = depthPrf->getExtrinsicTo(colorPrf);
    for(int i = 0; i < 3; ++i) {
        EXPECT_TRUE(std::isfinite(ex.trans[i]));
    }

    auto dot = [&](int r1, int r2) {
        return ex.rot[r1 * 3 + 0] * ex.rot[r2 * 3 + 0] + ex.rot[r1 * 3 + 1] * ex.rot[r2 * 3 + 1] + ex.rot[r1 * 3 + 2] * ex.rot[r2 * 3 + 2];
    };
    EXPECT_NEAR(dot(0, 0), 1.0f, 1e-3f);
    EXPECT_NEAR(dot(1, 1), 1.0f, 1e-3f);
    EXPECT_NEAR(dot(2, 2), 1.0f, 1e-3f);
    EXPECT_NEAR(dot(0, 1), 0.0f, 1e-3f);
    EXPECT_NEAR(dot(0, 2), 0.0f, 1e-3f);
    EXPECT_NEAR(dot(1, 2), 0.0f, 1e-3f);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_34_playback_calibration_camera_param_list) {
    auto pbDevice = createPlaybackDevice();
    auto list     = pbDevice->getCalibrationCameraParamList();
    ASSERT_NE(list, nullptr);
    ASSERT_GT(list->getCount(), 0u);

    auto param = list->getCameraParam(0);
    EXPECT_GT(param.depthIntrinsic.fx, 0.0f);
    EXPECT_GT(param.depthIntrinsic.fy, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.fx, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.fy, 0.0f);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_40_playback_decimation_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame";
    }

    ob::DecimationFilter filter;
    EXPECT_EQ(filter.getName(), "DecimationFilter");
    auto range = filter.getScaleRange();
    expectRangeValid(range);

    uint32_t inW = depthFrame->getWidth();
    uint32_t inH = depthFrame->getHeight();

    filter.setScaleValue(2);
    EXPECT_EQ(filter.getScaleValue(), 2);
    EXPECT_NEAR(filter.getConfigValue("decimate"), 2.0, 1e-6);

    auto out = filter.process(depthFrame);
    ASSERT_NE(out, nullptr);
    ASSERT_TRUE(out->is<ob::VideoFrame>());
    auto outV = out->as<ob::VideoFrame>();
    EXPECT_EQ(outV->getWidth(), inW / 2);
    EXPECT_EQ(outV->getHeight(), inH / 2);

    filter.enable(false);
    auto passthrough = filter.process(depthFrame);
    ASSERT_NE(passthrough, nullptr);
    auto passV = passthrough->as<ob::VideoFrame>();
    EXPECT_GT(passV->getWidth(), 0u);
    EXPECT_GT(passV->getHeight(), 0u);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_41_playback_threshold_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame";
    }

    ob::ThresholdFilter filter;
    EXPECT_EQ(filter.getName(), "ThresholdFilter");
    auto minR = filter.getMinRange();
    auto maxR = filter.getMaxRange();
    expectRangeValid(minR);
    expectRangeValid(maxR);
    EXPECT_TRUE(filter.setValueRange(100, 5000));
    EXPECT_FALSE(filter.setValueRange(5000, 100));
    EXPECT_FALSE(filter.setValueRange(3000, 3000));
    EXPECT_NEAR(filter.getConfigValue("min"), 100.0, 1e-6);
    EXPECT_NEAR(filter.getConfigValue("max"), 5000.0, 1e-6);

    auto out = filter.process(depthFrame);
    ASSERT_NE(out, nullptr);
    EXPECT_NE(out->getData(), nullptr);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_42_playback_format_converter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame";
    }

    ob::FormatConvertFilter filter;
    EXPECT_EQ(filter.getName(), "FormatConverter");

    if(depthFrame->getFormat() == OB_FORMAT_Y16) {
        filter.setFormatConvertType(FORMAT_Y16_TO_RGB);
    }
    else if(depthFrame->getFormat() == OB_FORMAT_YUYV) {
        filter.setFormatConvertType(FORMAT_YUYV_TO_RGB);
    }
    else {
        GTEST_SKIP() << "No known conversion mapping for depth format " << static_cast<int>(depthFrame->getFormat());
    }

    auto out = filter.process(depthFrame);
    ASSERT_NE(out, nullptr);
    EXPECT_NE(out->getFormat(), depthFrame->getFormat());
    EXPECT_NE(out->getData(), nullptr);
    EXPECT_GT(out->getDataSize(), 0u);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_43_playback_point_cloud_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame";
    }

    ob::PointCloudFilter filter;
    EXPECT_EQ(filter.getName(), "PointCloudFilter");

    auto decRange = filter.getDecimationFactorRange();
    expectRangeValid(decRange);
    filter.setCreatePointFormat(OB_FORMAT_POINT);
    filter.setCoordinateDataScaled(0.001f);
    filter.setCoordinateSystem(OB_LEFT_HAND_COORDINATE_SYSTEM);
    filter.setDecimationFactor(2);

    auto out = filter.process(depthFrame);
    ASSERT_NE(out, nullptr);
    ASSERT_TRUE(out->is<ob::PointsFrame>());
    auto points = out->as<ob::PointsFrame>();
    EXPECT_NE(points->getData(), nullptr);
    EXPECT_GT(points->getDataSize(), 0u);
    EXPECT_GT(points->getCoordinateValueScale(), 0.0f);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_44_playback_align_filter) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    ASSERT_NE(pipeline, nullptr);

    auto frames = waitForFramesetWithRetry(pipeline);
    if(!frames || !frames->getDepthFrame()) {
        for(int i = 0; i < 5 && (!frames || !frames->getDepthFrame()); ++i) {
            frames = waitForFramesetWithRetry(pipeline);
        }
    }
    pipeline->stop();
    ASSERT_NE(frames, nullptr);

    auto depth = frames->getDepthFrame();
    if(!depth) {
        GTEST_SKIP() << "No depth frame for align filter test";
    }

    ob::Align alignColor(OB_STREAM_COLOR);
    EXPECT_EQ(alignColor.getName(), "Align");
    EXPECT_EQ(alignColor.getAlignToStreamType(), OB_STREAM_COLOR);
    alignColor.setMatchTargetResolution(true);
    alignColor.setMatchTargetResolution(false);
    auto aligned = alignColor.process(frames);
    ASSERT_NE(aligned, nullptr);

    auto sensorList = pbDevice->getSensorList();
    auto colorPrf   = getFirstVideoProfile(sensorList, OB_SENSOR_COLOR);
    if(colorPrf) {
        EXPECT_NO_THROW(alignColor.setAlignToStreamProfile(colorPrf));
    }

    ob::Align alignDepth(OB_STREAM_DEPTH);
    EXPECT_EQ(alignDepth.getAlignToStreamType(), OB_STREAM_DEPTH);
    EXPECT_NO_THROW(alignDepth.process(frames));
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_45_playback_recommended_filters) {
    auto pbDevice   = createPlaybackDevice();
    auto sensorList = pbDevice->getSensorList();
    ASSERT_NE(sensorList, nullptr);
    if(!hasSensorType(sensorList, OB_SENSOR_DEPTH)) {
        GTEST_SKIP() << "No depth sensor for recommended filter test";
    }

    auto depthSensor = sensorList->getSensor(OB_SENSOR_DEPTH);
    ASSERT_NE(depthSensor, nullptr);
    auto filters = depthSensor->createRecommendedFilters();
    ASSERT_GT(filters.size(), 0u);

    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame for recommended filter process";
    }

    bool sawDecimation = false;
    for(const auto &f: filters) {
        ASSERT_NE(f, nullptr);
        EXPECT_FALSE(f->getName().empty());

        auto schema = f->getConfigSchemaVec();
        for(const auto &item: schema) {
            double v = f->getConfigValue(item.name);
            EXPECT_GE(v, item.min);
            EXPECT_LE(v, item.max);
        }

        f->enable(true);
        EXPECT_NO_THROW(f->process(depthFrame));
        EXPECT_NO_THROW(f->reset());

        if(f->getName() == "DecimationFilter") {
            sawDecimation = true;
            EXPECT_TRUE(f->is<ob::DecimationFilter>());
        }
    }
    EXPECT_TRUE(sawDecimation) << "DecimationFilter missing from recommended filters";
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_46_playback_filter_chain) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame";
    }

    ob::ThresholdFilter threshold;
    ASSERT_TRUE(threshold.setValueRange(100, 10000));
    ob::DecimationFilter decimation;
    decimation.setScaleValue(2);

    auto stage1 = threshold.process(depthFrame);
    ASSERT_NE(stage1, nullptr);
    auto stage2 = decimation.process(stage1);
    ASSERT_NE(stage2, nullptr);

    ASSERT_TRUE(stage1->is<ob::VideoFrame>());
    ASSERT_TRUE(stage2->is<ob::VideoFrame>());
    auto s1 = stage1->as<ob::VideoFrame>();
    auto s2 = stage2->as<ob::VideoFrame>();
    EXPECT_EQ(s2->getWidth(), s1->getWidth() / 2);
    EXPECT_EQ(s2->getHeight(), s1->getHeight() / 2);

    ob::FormatConvertFilter convert;
    if(stage2->getFormat() == OB_FORMAT_Y16) {
        convert.setFormatConvertType(FORMAT_Y16_TO_RGB);
        auto stage3 = convert.process(stage2);
        ASSERT_NE(stage3, nullptr);
        EXPECT_NE(stage3->getFormat(), OB_FORMAT_Y16);
    }

    auto gotAsync    = std::make_shared<std::promise<bool>>();
    auto asyncResult = gotAsync->get_future();
    decimation.setCallBack([gotAsync](std::shared_ptr<ob::Frame> frame) {
        if(frame) {
            try {
                gotAsync->set_value(true);
            }
            catch(const std::future_error &) {
            }
        }
    });
    decimation.pushFrame(depthFrame);
    EXPECT_EQ(asyncResult.wait_for(std::chrono::seconds(3)), std::future_status::ready);

    auto fromFactory = ob::FilterFactory::createFilter("DecimationFilter");
    ASSERT_NE(fromFactory, nullptr);
    EXPECT_EQ(fromFactory->getName(), "DecimationFilter");
    EXPECT_TRUE(fromFactory->is<ob::DecimationFilter>());
    EXPECT_NE(fromFactory->as<ob::DecimationFilter>(), nullptr);

    auto csv = fromFactory->getConfigSchema();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("decimate"), std::string::npos);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_47_playback_hdr_merge) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    auto filter = std::make_shared<ob::HdrMerge>();
    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->getName(), "HDRMerge");
    EXPECT_TRUE(filter->is<ob::HdrMerge>());
    EXPECT_NE(filter->as<ob::HdrMerge>(), nullptr);
    EXPECT_NO_THROW(filter->reset());

    std::shared_ptr<ob::Frame> out;
    EXPECT_NO_THROW(out = filter->process(depthFrame));
    if(out) {
        EXPECT_NE(out->getData(), nullptr);
    }
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_48_playback_sequence_id_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    auto filter = std::make_shared<ob::SequenceIdFilter>();
    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->getName(), "SequenceIdFilter");
    EXPECT_TRUE(filter->is<ob::SequenceIdFilter>());
    EXPECT_NE(filter->as<ob::SequenceIdFilter>(), nullptr);
    EXPECT_GE(filter->getSequenceIdListSize(), 2);
    EXPECT_NE(filter->getSequenceIdList(), nullptr);

    filter->selectSequenceId(0);
    EXPECT_EQ(filter->getSelectSequenceId(), 0);
    filter->selectSequenceId(1);
    EXPECT_EQ(filter->getSelectSequenceId(), 1);
    EXPECT_NEAR(filter->getConfigValue("sequenceid"), 1.0, 1e-6);

    auto out = filter->process(depthFrame);
    if(out) {
        EXPECT_EQ(out->getType(), OB_FRAME_DEPTH);
    }
    EXPECT_NO_THROW(filter->reset());
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_49_playback_spatial_advanced_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::SpatialAdvancedFilter>([&](const std::shared_ptr<ob::SpatialAdvancedFilter> &filter) {
        EXPECT_EQ(filter->getName(), "SpatialAdvancedFilter");
        expectRangeValid(filter->getAlphaRange());
        expectRangeValid(filter->getDispDiffRange());
        expectRangeValid(filter->getRadiusRange());
        expectRangeValid(filter->getMagnitudeRange());

        auto params      = filter->getFilterParams();
        params.alpha     = filter->getAlphaRange().def;
        params.disp_diff = filter->getDispDiffRange().def;
        params.magnitude = static_cast<uint8_t>(filter->getMagnitudeRange().def);
        params.radius    = filter->getRadiusRange().def;
        filter->setFilterParams(params);
        auto got = filter->getFilterParams();
        EXPECT_NEAR(got.alpha, params.alpha, 1e-6f);
        EXPECT_EQ(got.disp_diff, params.disp_diff);
        EXPECT_EQ(got.magnitude, params.magnitude);
        EXPECT_EQ(got.radius, params.radius);

        auto out = filter->process(depthFrame);
        if(out) {
            EXPECT_NE(out->getData(), nullptr);
        }
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_50_playback_spatial_fast_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::SpatialFastFilter>([&](const std::shared_ptr<ob::SpatialFastFilter> &filter) {
        EXPECT_EQ(filter->getName(), "SpatialFastFilter");
        auto radiusRange = filter->getRadiusRange();
        expectRangeValid(radiusRange);

        auto params   = filter->getFilterParams();
        params.radius = static_cast<uint8_t>(radiusRange.def);
        filter->setFilterParams(params);
        auto got = filter->getFilterParams();
        EXPECT_EQ(got.radius, params.radius);
        EXPECT_NEAR(filter->getConfigValue("radius"), static_cast<double>(params.radius), 1e-6);

        auto out = filter->process(depthFrame);
        if(out) {
            EXPECT_NE(out->getData(), nullptr);
        }
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_51_playback_spatial_moderate_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::SpatialModerateFilter>([&](const std::shared_ptr<ob::SpatialModerateFilter> &filter) {
        EXPECT_EQ(filter->getName(), "SpatialModerateFilter");
        auto magRange  = filter->getMagnitudeRange();
        auto radRange  = filter->getRadiusRange();
        auto diffRange = filter->getDispDiffRange();
        expectRangeValid(magRange);
        expectRangeValid(radRange);
        expectRangeValid(diffRange);

        auto params      = filter->getFilterParams();
        params.magnitude = static_cast<uint8_t>(magRange.def);
        params.radius    = static_cast<uint8_t>(radRange.def);
        params.disp_diff = static_cast<uint16_t>(diffRange.def);
        filter->setFilterParams(params);
        auto got = filter->getFilterParams();
        EXPECT_EQ(got.magnitude, params.magnitude);
        EXPECT_EQ(got.radius, params.radius);
        EXPECT_EQ(got.disp_diff, params.disp_diff);

        EXPECT_NO_THROW(filter->process(depthFrame));
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_52_playback_hole_filling_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::HoleFillingFilter>([&](const std::shared_ptr<ob::HoleFillingFilter> &filter) {
        EXPECT_EQ(filter->getName(), "HoleFillingFilter");

        filter->setFilterMode(OB_HOLE_FILL_TOP);
        EXPECT_EQ(filter->getFilterMode(), OB_HOLE_FILL_TOP);
        filter->setFilterMode(OB_HOLE_FILL_NEAREST);
        EXPECT_EQ(filter->getFilterMode(), OB_HOLE_FILL_NEAREST);
        filter->setFilterMode(OB_HOLE_FILL_FAREST);
        EXPECT_EQ(filter->getFilterMode(), OB_HOLE_FILL_FAREST);

        EXPECT_NO_THROW(filter->process(depthFrame));
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_53_playback_noise_removal_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::NoiseRemovalFilter>([&](const std::shared_ptr<ob::NoiseRemovalFilter> &filter) {
        EXPECT_EQ(filter->getName(), "NoiseRemovalFilter");
        auto dispRange = filter->getDispDiffRange();
        auto sizeRange = filter->getMaxSizeRange();
        expectRangeValid(dispRange);
        expectRangeValid(sizeRange);

        auto params      = filter->getFilterParams();
        params.disp_diff = dispRange.def;
        params.max_size  = sizeRange.def;
        filter->setFilterParams(params);
        auto got = filter->getFilterParams();
        EXPECT_EQ(got.disp_diff, params.disp_diff);
        EXPECT_EQ(got.max_size, params.max_size);
        EXPECT_NEAR(filter->getConfigValue("min_diff"), static_cast<double>(params.disp_diff), 1e-6);
        EXPECT_NEAR(filter->getConfigValue("max_size"), static_cast<double>(params.max_size), 1e-6);

        EXPECT_NO_THROW(filter->process(depthFrame));
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_54_playback_temporal_filter) {
    auto pbDevice = createPlaybackDevice();
    auto pipeline = createStartedPipeline(pbDevice);
    if(!pipeline) {
        GTEST_SKIP() << "Failed to start pipeline for temporal filter";
    }

    std::vector<std::shared_ptr<ob::DepthFrame>> depthFrames;
    for(int i = 0; i < 5; ++i) {
        auto frames = waitForFramesetWithRetry(pipeline);
        if(!frames) {
            continue;
        }
        auto depth = frames->getDepthFrame();
        if(depth) {
            depthFrames.push_back(depth);
        }
    }
    pipeline->stop();

    if(depthFrames.empty()) {
        GTEST_SKIP() << "No depth frames for temporal filter";
    }

    runPrivateFilterCase<ob::TemporalFilter>([&](const std::shared_ptr<ob::TemporalFilter> &filter) {
        EXPECT_EQ(filter->getName(), "TemporalFilter");
        auto diffRange = filter->getDiffScaleRange();
        auto wRange    = filter->getWeightRange();
        expectRangeValid(diffRange);
        expectRangeValid(wRange);

        filter->setDiffScale(diffRange.def);
        filter->setWeight(wRange.def);
        EXPECT_NEAR(filter->getConfigValue("diff_scale"), static_cast<double>(diffRange.def), 1e-6);
        EXPECT_NEAR(filter->getConfigValue("weight"), static_cast<double>(wRange.def), 1e-6);

        for(const auto &depth: depthFrames) {
            EXPECT_NO_THROW(filter->process(depth));
        }
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_55_playback_false_positive_filter) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::FalsePositiveFilter>([&](const std::shared_ptr<ob::FalsePositiveFilter> &filter) {
        EXPECT_EQ(filter->getName(), "FalsePositiveFilter");
        expectRangeValid(filter->getfpEdgeBleedFilterEnableRange());
        expectRangeValid(filter->getfpebfROIMinXRatioRange());
        expectRangeValid(filter->getfpebfROIMaxXRatioRange());
        expectRangeValid(filter->getfpebfROIMinYRatioRange());
        expectRangeValid(filter->getfpebfROIMaxYRatioRange());
        expectRangeValid(filter->getfpTextureSparsityFilterEnableRange());
        expectRangeValid(filter->getfptsfROIMinXRatioRange());
        expectRangeValid(filter->getfptsfROIMaxXRatioRange());
        expectRangeValid(filter->getfptsfROIMinYRatioRange());
        expectRangeValid(filter->getfptsfROIMaxYRatioRange());
        expectRangeValid(filter->getfptsfMaxNoiseLevelRange());
        expectRangeValid(filter->getfptsfMaxSpeckleSizeRange());
        expectRangeValid(filter->getfpPatternAmbiguityFilterEnableRange());
        expectRangeValid(filter->getfppafROIMinXRatioRange());
        expectRangeValid(filter->getfppafROIMaxXRatioRange());
        expectRangeValid(filter->getfppafROIMinYRatioRange());
        expectRangeValid(filter->getfppafROIMaxYRatioRange());
        expectRangeValid(filter->getfppafMaxNoiseLevelRange());
        expectRangeValid(filter->getfppafMaxSpeckleSizeRange());

        EXPECT_NO_THROW(filter->process(depthFrame));
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_56_playback_disparity_transform) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    runPrivateFilterCase<ob::DisparityTransform>([&](const std::shared_ptr<ob::DisparityTransform> &filter) {
        EXPECT_EQ(filter->getName(), "DisparityTransform");
        EXPECT_TRUE(filter->is<ob::DisparityTransform>());
        EXPECT_NE(filter->as<ob::DisparityTransform>(), nullptr);

        EXPECT_NO_THROW(filter->process(depthFrame));
        EXPECT_NO_THROW(filter->reset());
    });
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_60_playback_coord_transform_2d_to_3d) {
    auto pbDevice   = createPlaybackDevice();
    auto depthFrame = getDepthFrameFromPlayback(pbDevice);
    if(!depthFrame) {
        GTEST_SKIP() << "No depth frame in playback bag";
    }

    auto sensorList        = pbDevice->getSensorList();
    auto depthVideoProfile = getFirstVideoProfile(sensorList, OB_SENSOR_DEPTH);
    if(!depthVideoProfile) {
        GTEST_SKIP() << "No depth video stream profile in playback bag";
    }

    auto width  = depthFrame->getWidth();
    auto height = depthFrame->getHeight();
    ASSERT_GT(width, 0u);
    ASSERT_GT(height, 0u);
    ASSERT_EQ(depthFrame->getFormat(), OB_FORMAT_Y16);

    auto *data = reinterpret_cast<const uint16_t *>(depthFrame->getData());
    ASSERT_NE(data, nullptr);
    ASSERT_GE(depthFrame->getDataSize(), static_cast<uint64_t>(width) * height * sizeof(uint16_t))
        << "Depth frame buffer smaller than width*height*2 - truncated/corrupt bag";

    uint32_t cx       = width / 2;
    uint32_t cy       = height / 2;
    uint32_t idx      = cy * width + cx;
    uint16_t rawDepth = data[idx];
    if(rawDepth == 0) {
        for(uint32_t i = 0; i < width * height; ++i) {
            if(data[i] != 0) {
                rawDepth = data[i];
                idx      = i;
                cx       = idx % width;
                cy       = idx / width;
                break;
            }
        }
    }
    if(rawDepth == 0) {
        GTEST_SKIP() << "Depth frame contains only zero values";
    }

    float depthMm = rawDepth * depthFrame->getValueScale();
    ASSERT_GT(depthMm, 0.0f);

    auto        intrinsic = depthVideoProfile->getIntrinsic();
    OBExtrinsic identity  = {};
    identity.rot[0]       = 1.0f;
    identity.rot[4]       = 1.0f;
    identity.rot[8]       = 1.0f;

    OBPoint2f pixel   = { static_cast<float>(cx), static_cast<float>(cy) };
    OBPoint3f point3d = {};
    bool      ok      = ob::CoordinateTransformHelper::transformation2dto3d(pixel, depthMm, intrinsic, identity, &point3d);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(point3d.z, depthMm, depthMm * 0.05f + 1.0f);
}

TEST_P(TC_CPP_18_PlaybackParam, TC_CPP_18_61_playback_coord_transform_3d_to_2d) {
    auto pbDevice          = createPlaybackDevice();
    auto sensorList        = pbDevice->getSensorList();
    auto depthVideoProfile = getFirstVideoProfile(sensorList, OB_SENSOR_DEPTH);
    if(!depthVideoProfile) {
        GTEST_SKIP() << "No depth video stream profile in playback bag";
    }

    auto        intrinsic  = depthVideoProfile->getIntrinsic();
    auto        distortion = depthVideoProfile->getDistortion();
    OBExtrinsic identity   = {};
    identity.rot[0]        = 1.0f;
    identity.rot[4]        = 1.0f;
    identity.rot[8]        = 1.0f;

    OBPoint3f src   = { 0.0f, 0.0f, 1000.0f };
    OBPoint2f pixel = {};
    bool      ok    = ob::CoordinateTransformHelper::transformation3dto2d(src, intrinsic, distortion, identity, &pixel);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(pixel.x, intrinsic.cx, 2.0f);
    EXPECT_NEAR(pixel.y, intrinsic.cy, 2.0f);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TC_CPP_18_PlaybackParam);
INSTANTIATE_TEST_SUITE_P(AllPlaybackBags, TC_CPP_18_PlaybackParam, ::testing::ValuesIn(TestConfig::instance().allBagPaths()));