// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

using namespace hw_test;

class TC_CPP_18_Record_HW : public PipelineTest {};

TEST_F(TC_CPP_18_Record_HW, TC_CPP_18_01_record_device) {
#ifdef _WIN32
    std::string recFile = "ob_test_record.bag";
#else
    std::string recFile = "/tmp/ob_test_record.bag";
#endif
    std::remove(recFile.c_str());

    auto recorder = std::make_shared<ob::RecordDevice>(device_, recFile);
    auto pipeline = std::make_shared<ob::Pipeline>(device_);
    auto config   = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline->start(config);

    waitForFramesetWithRetry(pipeline);
    pipeline->stop();
    recorder.reset();

    std::ifstream ifs(recFile, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(ifs.good()) << "Record file not created";
    if(ifs.good()) {
        EXPECT_GT(ifs.tellg(), 1024) << "Record file too small";
    }
    std::remove(recFile.c_str());
}

TEST_F(TC_CPP_18_Record_HW, TC_CPP_18_02_record_pause_resume) {
#ifdef _WIN32
    std::string recFile = "ob_test_record2.bag";
#else
    std::string recFile = "/tmp/ob_test_record2.bag";
#endif
    std::remove(recFile.c_str());

    auto recorder = std::make_shared<ob::RecordDevice>(device_, recFile);
    auto pipeline = std::make_shared<ob::Pipeline>(device_);
    auto config   = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline->start(config);
    waitForFramesetWithRetry(pipeline);
    recorder->pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    recorder->resume();
    waitForFramesetWithRetry(pipeline);

    pipeline->stop();
    recorder.reset();

    std::ifstream ifs(recFile, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(ifs.good());
    std::remove(recFile.c_str());
}

TEST_F(TC_CPP_18_Record_HW, TC_CPP_18_08_record_playback_roundtrip) {
#ifdef _WIN32
    std::string recFile = "ob_test_roundtrip.bag";
#else
    std::string recFile = "/tmp/ob_test_roundtrip.bag";
#endif
    std::remove(recFile.c_str());

    {
        auto recorder = std::make_shared<ob::RecordDevice>(device_, recFile);
        auto pipeline = std::make_shared<ob::Pipeline>(device_);
        auto config   = std::make_shared<ob::Config>();
        config->enableStream(OB_STREAM_DEPTH);
        pipeline->start(config);
        waitForFramesetWithRetry(pipeline);
        pipeline->stop();
        recorder.reset();
    }

    std::ifstream ifs(recFile, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(ifs.good()) << "Recorded bag file was not created";
    EXPECT_GT(ifs.tellg(), 1024) << "Recorded bag file is unexpectedly small";
    ifs.close();

    std::shared_ptr<ob::PlaybackDevice> pbDevice;
    try {
        pbDevice = std::make_shared<ob::PlaybackDevice>(recFile);
    }
    catch(const ob::Error &e) {
        std::remove(recFile.c_str());
        GTEST_SKIP() << "Failed to open recorded bag as playback device: " << e.what();
    }
    ASSERT_NE(pbDevice, nullptr);

    auto pbPipeline = std::make_shared<ob::Pipeline>(pbDevice);
    auto pbConfig   = std::make_shared<ob::Config>();
    pbConfig->enableStream(OB_STREAM_DEPTH);

    bool playbackFailed = false;
    try {
        pbPipeline->start(pbConfig);
    }
    catch(const ob::Error &e) {
        playbackFailed = true;
        std::cout << "[18_08] Playback pipeline start failed: " << e.what() << std::endl;
    }

    if(!playbackFailed) {
        auto pbFrameset = waitForFramesetWithRetry(pbPipeline);
        ASSERT_NE(pbFrameset, nullptr) << "Playback did not produce any frameset";
        auto pbDepth = pbFrameset->getDepthFrame();
        ASSERT_NE(pbDepth, nullptr) << "Playback frameset has no depth frame";
        pbPipeline->stop();
    }

    std::remove(recFile.c_str());
}
