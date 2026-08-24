// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

using namespace hw_test;

class TC_CPP_24_Error_HW : public PipelineTest {};

TEST_F(TC_CPP_24_Error_HW, TC_CPP_24_04_pipeline_exception_safety) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    pipeline_->start(config);

    try {
        pipeline_->start(config);
    }
    catch(const ob::Error &) {
    }

    pipeline_->stop();

    EXPECT_NO_THROW(pipeline_->stop());

    EXPECT_NO_THROW(pipeline_->start(config));
    pipeline_->stop();
}

TEST_F(TC_CPP_24_Error_HW, TC_CPP_24_05_device_property_exception) {
    EXPECT_THROW(device_->getIntProperty(static_cast<OBPropertyID>(99999)), ob::Error);

    if(device_->isPropertySupported(OB_PROP_SLAVE_DEVICE_SYNC_STATUS_BOOL, OB_PERMISSION_READ)) {
        EXPECT_THROW(device_->setIntProperty(OB_PROP_SLAVE_DEVICE_SYNC_STATUS_BOOL, 0), ob::Error);
    }
}
