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
#include <vector>

using namespace hw_test;

#define HW_MAKE_FILTER_OR_SKIP(var, Type)                            \
    do {                                                             \
        try {                                                        \
            var = std::make_shared<Type>();                          \
        }                                                            \
        catch(const std::exception &) {                              \
            GTEST_SKIP() << #Type << " not available on this build"; \
        }                                                            \
    } while(0)

template <typename T> std::shared_ptr<T> hwMakeFilterOrNull() {
    try {
        return std::make_shared<T>();
    }
    catch(const std::exception &) {
        return nullptr;
    }
}

class TC_CPP_13_Filter_HW : public PipelineTest {
protected:
    std::shared_ptr<ob::DepthFrame> getDepthFrame() {
        auto config = std::make_shared<ob::Config>();
        config->enableStream(OB_STREAM_DEPTH);
        pipeline_->start(config);
        for(int i = 0; i < 3; i++)
            waitForFramesetWithRetry(pipeline_);
        auto fs = waitForFramesetWithRetry(pipeline_);
        pipeline_->stop();
        return fs ? fs->getDepthFrame() : nullptr;
    }
};

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_03_filter_sync_process) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::shared_ptr<ob::Filter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::DecimationFilter);
    auto result = filter->process(depth);
    EXPECT_NE(result, nullptr);
    EXPECT_GT(result->getDataSize(), 0u);
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_04_filter_async_callback) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::shared_ptr<ob::Filter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::DecimationFilter);
    std::atomic<bool> cbCalled{ false };
    filter->setCallBack([&cbCalled](std::shared_ptr<ob::Frame>) { cbCalled = true; });
    filter->pushFrame(depth);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(cbCalled.load());
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_08_pointcloud_filter) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::shared_ptr<ob::Filter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::PointCloudFilter);
    auto result = filter->process(depth);
    EXPECT_NE(result, nullptr);
    if(result) {
        EXPECT_GT(result->getDataSize(), 0u);
    }
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_09_align_filter) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    pipeline_->start(config);
    std::shared_ptr<ob::FrameSet> fs;
    for(int i = 0; i < 5; ++i) {
        fs = waitForFramesetWithRetry(pipeline_);
        if(fs && fs->getDepthFrame() && fs->getColorFrame())
            break;
    }
    ASSERT_NE(fs, nullptr);

    auto depth = fs->getDepthFrame();
    auto color = fs->getColorFrame();
    if(!depth || !color) {
        pipeline_->stop();
        GTEST_SKIP() << "Could not capture frameset with both depth and color for align test";
    }

    ob::Align                  align(OB_STREAM_COLOR);
    std::shared_ptr<ob::Frame> aligned;
    ASSERT_NO_THROW(aligned = align.process(fs));
    ASSERT_NE(aligned, nullptr);
    EXPECT_GT(aligned->getDataSize(), 0u);

    auto alignedFs = aligned->as<ob::FrameSet>();
    ASSERT_NE(alignedFs, nullptr);
    auto alignedDepth = alignedFs->getDepthFrame();
    ASSERT_NE(alignedDepth, nullptr);

    auto alignedVideo = alignedDepth->as<ob::VideoFrame>();
    ASSERT_NE(alignedVideo, nullptr);
    EXPECT_EQ(alignedVideo->getWidth(), color->getWidth());
    EXPECT_EQ(alignedVideo->getHeight(), color->getHeight());

    pipeline_->stop();
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_10_format_converter) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_COLOR);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);
    pipeline_->start(config);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    for(int i = 0; i < 5; i++)
        waitForFramesetWithRetry(pipeline_);

    std::shared_ptr<ob::ColorFrame> color;
    auto                            fs = waitForFramesetWithRetry(pipeline_);
    if(fs)
        color = fs->getColorFrame();

    pipeline_->stop();

    if(!color)
        GTEST_SKIP() << "No color frame produced by this device";

    std::shared_ptr<ob::FormatConvertFilter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::FormatConvertFilter);
    filter->process(color);
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_11_hdr_merge) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    auto hdrFilter = hwMakeFilterOrNull<ob::HdrMerge>();
    auto seqFilter = hwMakeFilterOrNull<ob::SequenceIdFilter>();
    if(!hdrFilter || !seqFilter) {
        GTEST_SKIP() << "HdrMerge or SequenceIdFilter not available";
    }

    auto seqResult = seqFilter->process(depth);
    if(seqResult) {
        auto hdrResult = hdrFilter->process(seqResult);
        if(hdrResult) {
            EXPECT_GT(hdrResult->getDataSize(), 0u);
        }
    }
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_12_decimation_filter) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::shared_ptr<ob::Filter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::DecimationFilter);
    auto result = filter->process(depth);
    EXPECT_NE(result, nullptr);
    if(result && result->is<ob::VideoFrame>()) {
        auto vf = result->as<ob::VideoFrame>();
        EXPECT_LE(vf->getWidth(), depth->getWidth());
    }
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_13_threshold_filter) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::shared_ptr<ob::Filter> filter;
    HW_MAKE_FILTER_OR_SKIP(filter, ob::ThresholdFilter);
    auto result = filter->process(depth);
    EXPECT_NE(result, nullptr);
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_14_spatial_filters) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::vector<std::shared_ptr<ob::Filter>> filters;
    if(auto f = hwMakeFilterOrNull<ob::SpatialAdvancedFilter>())
        filters.push_back(f);
    if(auto f = hwMakeFilterOrNull<ob::SpatialFastFilter>())
        filters.push_back(f);
    if(auto f = hwMakeFilterOrNull<ob::SpatialModerateFilter>())
        filters.push_back(f);

    for(const auto &f: filters) {
        auto result = f->process(depth);
        if(result) {
            EXPECT_GT(result->getDataSize(), 0u);
        }
    }
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_15_temporal_holefilling_noise) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::vector<std::shared_ptr<ob::Filter>> filters;
    if(auto f = hwMakeFilterOrNull<ob::TemporalFilter>())
        filters.push_back(f);
    if(auto f = hwMakeFilterOrNull<ob::HoleFillingFilter>())
        filters.push_back(f);
    if(auto f = hwMakeFilterOrNull<ob::NoiseRemovalFilter>())
        filters.push_back(f);

    ASSERT_FALSE(filters.empty()) << "No temporal/holefilling/noise filters available";

    for(const auto &f: filters) {
        auto result = f->process(depth);
        ASSERT_NE(result, nullptr);
        EXPECT_GT(result->getDataSize(), 0u);
    }
}

TEST_F(TC_CPP_13_Filter_HW, TC_CPP_13_16_false_positive_disparity) {
    auto depth = getDepthFrame();
    ASSERT_NE(depth, nullptr);

    std::vector<std::shared_ptr<ob::Filter>> filters;
    if(auto f = hwMakeFilterOrNull<ob::FalsePositiveFilter>())
        filters.push_back(f);
    if(auto f = hwMakeFilterOrNull<ob::DisparityTransform>())
        filters.push_back(f);

    ASSERT_FALSE(filters.empty()) << "No false-positive/disparity filters available";

    for(const auto &f: filters) {
        auto result = f->process(depth);
        ASSERT_NE(result, nullptr);
        EXPECT_GT(result->getDataSize(), 0u);
    }
}
