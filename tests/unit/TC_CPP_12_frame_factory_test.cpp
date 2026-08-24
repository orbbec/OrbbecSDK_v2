// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_12_FrameFactory : public SDKTestBase {};

TEST_F(TC_CPP_12_FrameFactory, TC_CPP_12_01_create_frame_and_video_frame) {
    auto frame = ob::FrameFactory::createFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 640 * 480 * 2);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->getType(), OB_FRAME_DEPTH);
    EXPECT_EQ(frame->getFormat(), OB_FORMAT_Y16);
    EXPECT_EQ(frame->getDataSize(), 640 * 480 * 2);

    auto vframe = ob::FrameFactory::createVideoFrame(OB_FRAME_COLOR, OB_FORMAT_RGB, 640, 480);
    ASSERT_NE(vframe, nullptr);
    EXPECT_EQ(vframe->getType(), OB_FRAME_COLOR);
    EXPECT_EQ(vframe->getFormat(), OB_FORMAT_RGB);
    EXPECT_EQ(vframe->getWidth(), 640u);
    EXPECT_EQ(vframe->getHeight(), 480u);
}

TEST_F(TC_CPP_12_FrameFactory, TC_CPP_12_03_create_frame_from_buffer) {
    bool destroyCalled = false;

    const uint32_t bufSize = 100;
    auto          *buffer  = new uint8_t[bufSize];
    std::memset(buffer, 0x42, bufSize);

    auto frame = ob::FrameFactory::createFrameFromBuffer(
        OB_FRAME_DEPTH, OB_FORMAT_Y16, buffer,
        [&destroyCalled](uint8_t *buf) {
            delete[] buf;
            destroyCalled = true;
        },
        bufSize);
    ASSERT_NE(frame, nullptr);

    auto *data = frame->getData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data[0], 0x42);

    frame = nullptr;
    EXPECT_TRUE(destroyCalled) << "Custom destroy callback was not invoked";
}

TEST_F(TC_CPP_12_FrameFactory, TC_CPP_12_04_create_empty_frameset) {
    auto frameset = ob::FrameFactory::createFrameSet();
    ASSERT_NE(frameset, nullptr);
    EXPECT_EQ(frameset->getCount(), 0u);

    auto frame = ob::FrameFactory::createFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 640 * 480 * 2);
    ASSERT_NE(frame, nullptr);

    frameset->pushFrame(frame);
    EXPECT_EQ(frameset->getCount(), 1u);
}
