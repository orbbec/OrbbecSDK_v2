// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_17_Interleave : public DeviceTest {};

TEST_F(TC_CPP_17_Interleave, TC_CPP_17_01_support_query) {
    bool supported = device_->isFrameInterleaveSupported();
    if(!supported)
        GTEST_SKIP() << "Frame interleave not supported";

    auto list = device_->getAvailableFrameInterleaveList();
    ASSERT_NE(list, nullptr);
    EXPECT_GT(list->getCount(), 0u);
    for(uint32_t i = 0; i < list->getCount(); ++i) {
        EXPECT_NE(list->getName(i), nullptr);
    }
}

TEST_F(TC_CPP_17_Interleave, TC_CPP_17_02_load_interleave) {
    if(!device_->isFrameInterleaveSupported())
        GTEST_SKIP() << "Frame interleave not supported";
    auto list = device_->getAvailableFrameInterleaveList();
    if(list && list->getCount() > 0) {
        FrameInterleaveGuard guard(device_);
        ASSERT_NO_THROW(device_->loadFrameInterleave(list->getName(0)));
    }
}

TEST_F(TC_CPP_17_Interleave, TC_CPP_17_03_interleave_stream) {
    if(!device_->isFrameInterleaveSupported())
        GTEST_SKIP() << "Frame interleave not supported";
    auto list = device_->getAvailableFrameInterleaveList();
    if(!list || list->getCount() == 0)
        GTEST_SKIP() << "No frame interleave list available";

    FrameInterleaveGuard guard(device_);
    ASSERT_NO_THROW(device_->loadFrameInterleave(list->getName(0)));

    auto       pipeline = std::make_shared<ob::Pipeline>(device_);
    ScopeGuard stopPipeline([&] {
        try {
            pipeline->stop();
        }
        catch(...) {
        }
    });
    auto       config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    ASSERT_NO_THROW(pipeline->start(config));

    int got = 0;
    for(int i = 0; i < 5; i++) {
        if(waitForFramesetWithRetry(pipeline))
            ++got;
    }
    ASSERT_GT(got, 0) << "Interleave stream produced no frames";
}