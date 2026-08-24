// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_10_Frame : public SDKTestBase {};

TEST_F(TC_CPP_10_Frame, TC_CPP_10_04_frame_ref_count) {
    const uint32_t bufSize = 100;
    auto          *buffer  = new uint8_t[bufSize];
    std::memset(buffer, 0x42, bufSize);

    bool                       destroyed = false;
    std::shared_ptr<ob::Frame> frame;
    {
        frame = ob::FrameFactory::createFrameFromBuffer(
            OB_FRAME_DEPTH, OB_FORMAT_Y16, buffer,
            [&destroyed](uint8_t *buf) {
                delete[] buf;
                destroyed = true;
            },
            bufSize);
        ASSERT_NE(frame, nullptr);

        auto ref = frame;
        ASSERT_NE(ref, nullptr);
        EXPECT_EQ(ref->getDataSize(), bufSize);
        EXPECT_FALSE(destroyed);
    }
    EXPECT_EQ(frame->getDataSize(), bufSize);
    EXPECT_FALSE(destroyed);

    frame = nullptr;
    EXPECT_TRUE(destroyed) << "Frame should be released when all references are dropped";
}

TEST_F(TC_CPP_10_Frame, TC_CPP_10_13_frameset_push_frame) {
    auto frameset = ob::FrameFactory::createFrameSet();
    ASSERT_NE(frameset, nullptr);

    auto depthFrame = ob::FrameFactory::createFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 640 * 480 * 2);
    ASSERT_NE(depthFrame, nullptr);

    frameset->pushFrame(depthFrame);
    EXPECT_EQ(frameset->getCount(), 1u) << "Frameset count should be exactly 1 after flashing one frame";
}
