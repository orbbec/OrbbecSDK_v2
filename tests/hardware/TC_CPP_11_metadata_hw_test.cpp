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

class TC_CPP_11_Metadata_HW : public PipelineTest {};

TEST_F(TC_CPP_11_Metadata_HW, TC_CPP_11_01_metadata_basic_read) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto depth = fs->getDepthFrame();
    ASSERT_NE(depth, nullptr);

    if(depth->hasMetadata(OB_FRAME_METADATA_TYPE_TIMESTAMP)) {
        auto ts = depth->getMetadataValue(OB_FRAME_METADATA_TYPE_TIMESTAMP);
        EXPECT_GT(ts, 0);
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_11_Metadata_HW, TC_CPP_11_02_frame_number_fps) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    for(int i = 0; i < 5; i++)
        waitForFramesetWithRetry(pipeline_);

    std::vector<int64_t> frameNums;
    for(int i = 0; i < 5; i++) {
        auto fs = waitForFramesetWithRetry(pipeline_);
        if(!fs)
            continue;
        auto d = fs->getDepthFrame();
        if(d && d->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER)) {
            d->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
        }
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_11_Metadata_HW, TC_CPP_11_03_exposure_gain_laser) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    for(int i = 0; i < 5; i++)
        waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto d = fs->getDepthFrame();
    ASSERT_NE(d, nullptr);

    if(d->hasMetadata(OB_FRAME_METADATA_TYPE_EXPOSURE)) {
        auto exp = d->getMetadataValue(OB_FRAME_METADATA_TYPE_EXPOSURE);
        EXPECT_GT(exp, 0);
    }
    if(d->hasMetadata(OB_FRAME_METADATA_TYPE_GAIN)) {
        auto gain = d->getMetadataValue(OB_FRAME_METADATA_TYPE_GAIN);
        EXPECT_GE(gain, 0);
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_11_Metadata_HW, TC_CPP_11_04_raw_metadata_buffer) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    for(int i = 0; i < 3; i++)
        waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto d = fs->getDepthFrame();
    ASSERT_NE(d, nullptr);

    auto md     = d->getMetadata();
    auto mdSize = d->getMetadataSize();
    if(mdSize > 0) {
        EXPECT_NE(md, nullptr);
    }
    pipeline_->stop();
}

TEST_F(TC_CPP_11_Metadata_HW, TC_CPP_11_05_unsupported_field_safe) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    pipeline_->start(config);
    for(int i = 0; i < 3; i++)
        waitForFramesetWithRetry(pipeline_);
    auto fs = waitForFramesetWithRetry(pipeline_);
    ASSERT_NE(fs, nullptr);
    auto d = fs->getDepthFrame();
    ASSERT_NE(d, nullptr);

    bool has = d->hasMetadata(static_cast<OBFrameMetadataType>(9999));
    EXPECT_FALSE(has);
    pipeline_->stop();
}