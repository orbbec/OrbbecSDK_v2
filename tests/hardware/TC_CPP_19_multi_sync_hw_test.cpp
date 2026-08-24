// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cmath>
#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_19_MultiSync : public DeviceTest {};

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_01_sync_mode_support) {
    if(!supportsMultiDeviceSync(device_))
        GTEST_SKIP() << "Multi-device sync not supported on this device";
    auto bitmap = device_->getSupportedMultiDeviceSyncModeBitmap();
    EXPECT_NE(bitmap, 0u) << "No sync modes supported";

    auto config = device_->getMultiDeviceSyncConfig();
    EXPECT_TRUE(bitmap & config.syncMode) << "Current syncMode not in supported bitmap";

    EXPECT_TRUE(std::isfinite(static_cast<float>(config.depthDelayUs)));
    EXPECT_TRUE(std::isfinite(static_cast<float>(config.colorDelayUs)));
    EXPECT_TRUE(std::isfinite(static_cast<float>(config.trigger2ImageDelayUs)));
    EXPECT_TRUE(std::isfinite(static_cast<float>(config.triggerOutDelayUs)));
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_02_freerun_standalone) {
    if(!supportsMultiDeviceSync(device_))
        GTEST_SKIP() << "Multi-device sync not supported on this device";
    SyncConfigGuard guard(device_);

    OBMultiDeviceSyncConfig config = {};
    config.syncMode                = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    ASSERT_NO_THROW(device_->setMultiDeviceSyncConfig(config));
    auto readBack = device_->getMultiDeviceSyncConfig();
    EXPECT_EQ(readBack.syncMode, OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN);

    config.syncMode = OB_MULTI_DEVICE_SYNC_MODE_STANDALONE;
    ASSERT_NO_THROW(device_->setMultiDeviceSyncConfig(config));
    readBack = device_->getMultiDeviceSyncConfig();
    EXPECT_EQ(readBack.syncMode, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE);
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_03_primary_secondary) {
    if(ENV().deviceCount() < 2) {
        GTEST_SKIP() << "Test requires >= 2 devices, available: " << ENV().deviceCount();
    }

    auto devList = ctx_->queryDeviceList();
    ASSERT_GE(devList->getCount(), 2u);

    auto devA = devList->getDevice(0);
    auto devB = devList->getDevice(1);
    ASSERT_NE(devA, nullptr);
    ASSERT_NE(devB, nullptr);

    SyncConfigGuard guardA(devA);
    SyncConfigGuard guardB(devB);

    OBMultiDeviceSyncConfig configA = {};
    configA.syncMode                = OB_MULTI_DEVICE_SYNC_MODE_PRIMARY;
    configA.triggerOutEnable        = true;
    configA.framesPerTrigger        = 1;
    ASSERT_NO_THROW(devA->setMultiDeviceSyncConfig(configA));
    auto readA = devA->getMultiDeviceSyncConfig();
    EXPECT_EQ(readA.syncMode, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY);

    OBMultiDeviceSyncConfig configB = {};
    configB.syncMode                = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
    configB.framesPerTrigger        = 1;
    ASSERT_NO_THROW(devB->setMultiDeviceSyncConfig(configB));
    auto readB = devB->getMultiDeviceSyncConfig();
    EXPECT_EQ(readB.syncMode, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY);

    auto       pipelineA = std::make_shared<ob::Pipeline>(devA);
    auto       pipelineB = std::make_shared<ob::Pipeline>(devB);
    ScopeGuard stopPipelines([&] {
        try {
            pipelineA->stop();
        }
        catch(...) {
        }
        try {
            pipelineB->stop();
        }
        catch(...) {
        }
    });

    auto cfg = std::make_shared<ob::Config>();
    cfg->enableStream(OB_STREAM_DEPTH);

    ASSERT_NO_THROW(pipelineA->start(cfg));
    ASSERT_NO_THROW(pipelineB->start(cfg));

    auto fsA = waitForFramesetWithRetry(pipelineA);
    auto fsB = waitForFramesetWithRetry(pipelineB);
    ASSERT_NE(fsA, nullptr);
    ASSERT_NE(fsB, nullptr);
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_04_software_trigger) {
    auto bitmap = device_->getSupportedMultiDeviceSyncModeBitmap();
    if(!(bitmap & OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING)) {
        GTEST_SKIP() << "Software triggering not supported";
    }
    if(!device_->isPropertySupported(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, OB_PERMISSION_WRITE)) {
        GTEST_SKIP() << "Capture trigger property is not writable on this device";
    }

    SyncConfigGuard guard(device_);

    OBMultiDeviceSyncConfig config = {};
    config.syncMode                = OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING;
    config.framesPerTrigger        = 1;
    device_->setMultiDeviceSyncConfig(config);

    auto       pipeline = std::make_shared<ob::Pipeline>(device_);
    ScopeGuard stopPipeline([&] {
        try {
            pipeline->stop();
        }
        catch(...) {
        }
    });
    auto       cfg = std::make_shared<ob::Config>();
    cfg->enableStream(OB_STREAM_DEPTH);
    pipeline->start(cfg);

    int frameCount = 0;
    for(int i = 0; i < 3; ++i) {
        device_->triggerCapture();
        auto fs = waitForFramesetWithRetry(pipeline);
        if(fs)
            frameCount++;
    }

    EXPECT_GE(frameCount, 1) << "Software trigger failed to produce any frames";
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_05_timestamp_reset) {
    auto origConfig = device_->getTimestampResetConfig();

    OBDeviceTimestampResetConfig testConfig = origConfig;
    testConfig.enable                       = !origConfig.enable;

    ScopeGuard restoreConfig([&] {
        try {
            device_->setTimestampResetConfig(origConfig);
        }
        catch(...) {
        }
    });

    ASSERT_NO_THROW(device_->setTimestampResetConfig(testConfig));
    auto readBack = device_->getTimestampResetConfig();
    EXPECT_EQ(readBack.enable, testConfig.enable);

    try {
        device_->timestampReset();
    }
    catch(const ob::Error &e) {
        GTEST_SKIP() << "timestampReset not supported: " << e.what();
    }
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_06_timer_sync_host) {
    if(!supportsMultiDeviceSync(device_))
        GTEST_SKIP() << "Multi-device sync not supported on this device";
    ASSERT_NO_THROW(device_->timerSyncWithHost());
}

TEST_F(TC_CPP_19_MultiSync, TC_CPP_19_07_sync_config_fields) {
    if(!supportsMultiDeviceSync(device_))
        GTEST_SKIP() << "Multi-device sync not supported on this device";
    SyncConfigGuard guard(device_);

    OBMultiDeviceSyncConfig config = {};
    config.syncMode                = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    config.depthDelayUs            = 100;
    config.colorDelayUs            = 200;
    config.trigger2ImageDelayUs    = 0;
    config.triggerOutEnable        = false;
    config.triggerOutDelayUs       = 0;
    config.framesPerTrigger        = 1;

    ASSERT_NO_THROW(device_->setMultiDeviceSyncConfig(config));
    auto readBack = device_->getMultiDeviceSyncConfig();
    EXPECT_EQ(readBack.syncMode, OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN);
}
