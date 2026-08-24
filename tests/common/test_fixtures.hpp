// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "test_config.hpp"
#include "test_utils.hpp"

#include <libobsensor/ObSensor.hpp>
#include <gtest/gtest.h>

#include <string>

class SDKTestBase : public ::testing::Test {
protected:
    const TestConfig &cfg_ = TestConfig::instance();
};

class ContextTest : public SDKTestBase {
protected:
    std::shared_ptr<ob::Context> ctx_;

    void SetUp() override {
        ctx_ = std::make_shared<ob::Context>();
        ASSERT_NE(ctx_, nullptr);
    }

    void TearDown() override {
        ctx_.reset();
    }
};

class HardwareTest : public SDKTestBase {
protected:
    int         caseStartPid_ = 0;
    std::string caseStartType_;
    bool        setupCompleted_ = false;

    void SetUp() override {
        if(!test_utils::tryGetDeviceState(cfg_.deviceSerial(), caseStartPid_, caseStartType_)) {
            GTEST_SKIP() << "No connected device detected before test setup";
        }
        setupCompleted_ = true;
    }

    void TearDown() override {
        if(!setupCompleted_)
            return;

        int         endPid = 0;
        std::string endType;
        ASSERT_TRUE(test_utils::tryGetDeviceState(cfg_.deviceSerial(), endPid, endType)) << "Device is unavailable during teardown";
        EXPECT_EQ(endPid, caseStartPid_) << "Device PID changed during test; start: " << test_utils::pidToHex(caseStartPid_)
                                         << ", end: " << test_utils::pidToHex(endPid);
        EXPECT_EQ(endType, caseStartType_) << "Device type changed; start: " << caseStartType_ << ", end: " << endType;
    }
};

class DeviceTest : public HardwareTest {
protected:
    std::shared_ptr<ob::Context>    ctx_;
    std::shared_ptr<ob::Device>     device_;
    std::shared_ptr<ob::DeviceInfo> devInfo_;
    bool                            deviceSetupCompleted_ = false;

    void SetUp() override {
        HardwareTest::SetUp();
        if(!setupCompleted_)
            return;
        ctx_ = std::make_shared<ob::Context>();
        ASSERT_NE(ctx_, nullptr);

        auto devList = ctx_->queryDeviceList();
        ASSERT_NE(devList, nullptr);
        ASSERT_GT(devList->deviceCount(), 0u) << "No connected device";

        const std::string &sn = cfg_.deviceSerial();
        if(!sn.empty()) {
            try {
                device_ = devList->getDeviceBySN(sn.c_str());
            }
            catch(const ob::Error &) {
                GTEST_SKIP() << "Device SN '" << sn << "' not found";
            }
        }
        else {
            device_ = devList->getDevice(0);
        }
        ASSERT_NE(device_, nullptr);

        devInfo_ = device_->getDeviceInfo();
        ASSERT_NE(devInfo_, nullptr);
        deviceSetupCompleted_ = true;
    }

    void TearDown() override {
        devInfo_.reset();
        device_.reset();
        ctx_.reset();
        HardwareTest::TearDown();
    }

    bool hasFirmware() const {
        return cfg_.hasFirmware();
    }

    bool hasDepthPreset() const {
        return cfg_.hasDepthPreset();
    }

    bool isDestructiveAllowed() const {
        return cfg_.allowDestructive();
    }

    bool hasNetworkConfig() const {
        return !cfg_.netIp().empty() && !cfg_.netMac().empty();
    }

    std::shared_ptr<ob::Device> getNetDevice() {
        const std::string &ip = cfg_.netIp();
        if(!ip.empty()) {
            try {
                return ctx_->createNetDevice(ip.c_str(), 8090);
            }
            catch(...) {
            }
        }
        auto devList = ctx_->queryDeviceList();
        if(devList) {
            for(uint32_t i = 0; i < devList->getCount(); ++i) {
                if(std::string(devList->getConnectionType(i)) == "Ethernet") {
                    return devList->getDevice(i);
                }
            }
        }
        return nullptr;
    }
};

class PipelineTest : public DeviceTest {
protected:
    std::shared_ptr<ob::Pipeline> pipeline_;

    void SetUp() override {
        DeviceTest::SetUp();
        if(!deviceSetupCompleted_)
            return;
        pipeline_ = std::make_shared<ob::Pipeline>(device_);
    }

    void TearDown() override {
        if(pipeline_) {
            try {
                pipeline_->stop();
            }
            catch(...) {
            }
        }
        pipeline_.reset();
        DeviceTest::TearDown();
    }
};

class SensorTest : public DeviceTest {
protected:
    std::shared_ptr<ob::SensorList> sensorList_;

    void SetUp() override {
        DeviceTest::SetUp();
        if(!deviceSetupCompleted_)
            return;
        sensorList_ = device_->getSensorList();
        ASSERT_NE(sensorList_, nullptr);
    }

    void TearDown() override {
        if(sensorList_ && device_) {
            for(uint32_t i = 0; i < sensorList_->getCount(); ++i) {
                auto sensor = sensorList_->getSensor(i);
                if(sensor) {
                    try {
                        sensor->stop();
                    }
                    catch(...) {
                    }
                }
            }
        }
        sensorList_.reset();
        DeviceTest::TearDown();
    }
};
