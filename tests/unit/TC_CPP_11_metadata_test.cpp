// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_11_Metadata : public SDKTestBase {};

TEST_F(TC_CPP_11_Metadata, TC_CPP_11_06_update_metadata_c_api) {
    auto frame = ob::FrameFactory::createFrame(OB_FRAME_DEPTH, OB_FORMAT_Y16, 640 * 480 * 2);
    ASSERT_NE(frame, nullptr);

    uint8_t metadata[64] = {};
    for(uint32_t i = 0; i < sizeof(metadata); ++i) {
        metadata[i] = static_cast<uint8_t>(i);
    }
    frame->updateMetadata(metadata, sizeof(metadata));

    auto mdPtr = frame->getMetadata();
    ASSERT_NE(mdPtr, nullptr) << "Metadata pointer should not be null after update";
    for(uint32_t i = 0; i < sizeof(metadata); ++i) {
        EXPECT_EQ(mdPtr[i], metadata[i]) << "Metadata mismatch at index " << i;
    }
}
