// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include <cstring>
#include <memory>
#include <string>

class TC_CPP_02_Discovery : public ContextTest {};

TEST_F(TC_CPP_02_Discovery, TC_CPP_02_06_clock_sync) {

    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(1000));
    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(0));
    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(500));

    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr) << "Context functional broken after clock sync toggling";

    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(0));
}