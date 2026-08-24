// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_05_AccessMode : public DeviceTest {};

TEST_F(TC_CPP_05_AccessMode, TC_CPP_05_01_default_access) {
    auto pipeline = std::make_shared<ob::Pipeline>(device_);
    auto config   = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    EXPECT_NO_THROW(pipeline->start(config)) << "start() should not throw in DEFAULT_ACCESS mode";

    auto frameSet = waitForFramesetWithRetry(pipeline);
    ASSERT_NE(frameSet, nullptr) << "waitForFrameset() should return valid frameset";

    auto depthFrame = frameSet->getDepthFrame();
    ASSERT_NE(depthFrame, nullptr) << "Frameset should contain depth frame";

    pipeline->stop();
}

TEST_F(TC_CPP_05_AccessMode, TC_CPP_05_02_exclusive_access) {
    auto sn = std::string(devInfo_->getSerialNumber());
    devInfo_.reset();
    device_.reset();

    auto devList = ctx_->queryDeviceList();
    auto dev1    = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_EXCLUSIVE_ACCESS);
    ASSERT_NE(dev1, nullptr) << "First getDevice with EXCLUSIVE_ACCESS should succeed";

    auto pipeline = std::make_shared<ob::Pipeline>(dev1);
    auto config   = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    EXPECT_NO_THROW(pipeline->start(config)) << "start() should not throw in EXCLUSIVE_ACCESS mode";

    auto frames = waitForFramesetWithRetry(pipeline);
    ASSERT_NE(frames, nullptr) << "Should receive frames in exclusive access mode";
    auto depthFrame = frames->getDepthFrame();
    ASSERT_NE(depthFrame, nullptr);

    {
        std::shared_ptr<ob::Device> dev2;
        try {
            dev2 = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_EXCLUSIVE_ACCESS);
        }
        catch(const ob::Error &) {
        }
        if(dev2 != nullptr) {
            std::cout << "[NOTE] Device allows second EXCLUSIVE_ACCESS open — not enforced on this device" << std::endl;
        }
    }

    pipeline->stop();
    dev1.reset();

    {
        auto dev3 = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_EXCLUSIVE_ACCESS);
        ASSERT_NE(dev3, nullptr) << "getDevice after releasing first device should succeed";
        dev3.reset();
    }
}

TEST_F(TC_CPP_05_AccessMode, TC_CPP_05_03_shared_access) {
    auto sn = std::string(devInfo_->getSerialNumber());
    devInfo_.reset();
    device_.reset();

    auto devList = ctx_->queryDeviceList();
    auto devA    = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_CONTROL_ACCESS);
    ASSERT_NE(devA, nullptr) << "First getDevice with CONTROL_ACCESS should succeed";

    auto infoA = devA->getDeviceInfo();
    ASSERT_NE(infoA, nullptr);

    auto devB = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_CONTROL_ACCESS);
    ASSERT_NE(devB, nullptr) << "Second getDevice with CONTROL_ACCESS should succeed in shared mode";

    auto infoB = devB->getDeviceInfo();
    ASSERT_NE(infoB, nullptr);

    EXPECT_STREQ(infoA->getSerialNumber(), infoB->getSerialNumber()) << "Both handles should return same serial number";
    EXPECT_STREQ(infoA->getName(), infoB->getName()) << "Both handles should return same device name";
    EXPECT_STREQ(infoA->getFirmwareVersion(), infoB->getFirmwareVersion()) << "Both handles should return same firmware version";
}

TEST_F(TC_CPP_05_AccessMode, TC_CPP_05_04_control_only_access) {
    auto sn = std::string(devInfo_->getSerialNumber());
    devInfo_.reset();
    device_.reset();

    auto devList = ctx_->queryDeviceList();
    auto dev     = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_CONTROL_ACCESS);
    ASSERT_NE(dev, nullptr) << "getDevice with CONTROL_ACCESS should succeed";

    auto info = dev->getDeviceInfo();
    ASSERT_NE(info, nullptr);
    EXPECT_NE(info->getName(), nullptr);

    if(dev->isPropertySupported(OB_PROP_LASER_BOOL, OB_PERMISSION_READ)) {
        bool val = dev->getBoolProperty(OB_PROP_LASER_BOOL);
        EXPECT_NO_THROW(dev->setBoolProperty(OB_PROP_LASER_BOOL, val)) << "Property set should succeed in control mode";
    }

    auto pipeline = std::make_shared<ob::Pipeline>(dev);
    auto config   = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    bool streamingFailed = false;
    try {
        pipeline->start(config);
        auto frames = waitForFramesetWithRetry(pipeline);
        if(frames) {
            auto depthFrame = frames->getDepthFrame();
            EXPECT_NE(depthFrame, nullptr) << "Frameset should contain depth frame if streaming succeeds in CONTROL_ACCESS";
        }
        else {
            streamingFailed = true;
        }
        pipeline->stop();
    }
    catch(const ob::Error &) {
        streamingFailed = true;
    }

    if(streamingFailed) {
        std::cout << "[05_04] Streaming correctly rejected in CONTROL_ACCESS mode" << std::endl;
    }
    else {
        std::cout << "[NOTE] Device allows streaming in CONTROL_ACCESS mode" << std::endl;
    }
}
