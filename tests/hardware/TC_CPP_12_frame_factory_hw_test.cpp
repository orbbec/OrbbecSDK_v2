// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_12_FrameFactory_HW : public PipelineTest {};

TEST_F(TC_CPP_12_FrameFactory_HW, TC_CPP_12_02_clone_frame) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    for(int i = 0; i < 3; i++)
        waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);
    pipeline_->stop();

    auto clone = ob::FrameFactory::createFrameFromOtherFrame(depth);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->getDataSize(), depth->getDataSize());
    EXPECT_EQ(clone->getFormat(), depth->getFormat());
}
