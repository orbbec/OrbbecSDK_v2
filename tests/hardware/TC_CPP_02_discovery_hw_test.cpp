// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace hw_test;

class TC_CPP_02_Discovery_HW : public DeviceTest {};

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_01_usb_device_enum) {
    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    ASSERT_GT(devList->getCount(), 0u);

    for(uint32_t i = 0; i < devList->getCount(); i++) {
        EXPECT_NE(devList->getName(i), nullptr);
        EXPECT_NE(devList->getSerialNumber(i), nullptr);
    }
}

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_02_net_device_enum_toggle) {
    if(!hasNetworkConfig()) {
        GTEST_SKIP() << "network.ip and network.mac must be set in config";
    }

    const std::string netIp        = ENV().netIp();
    auto              hasNetDevice = [&](const std::shared_ptr<ob::DeviceList> &devList) {
        if(!devList) {
            return false;
        }

        for(uint32_t i = 0; i < devList->getCount(); ++i) {
            const char *ip = devList->getIpAddress(i);
            if(ip && netIp == ip) {
                return true;
            }
        }

        return false;
    };

    ASSERT_NO_THROW(ctx_->enableNetDeviceEnumeration(true));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    ASSERT_GT(devList->getCount(), 0u) << "No devices enumerated after enabling network discovery";
    EXPECT_TRUE(hasNetDevice(devList)) << "Configured network device (ip=" << netIp << ") was not found after enabling network discovery";

    ASSERT_NO_THROW(ctx_->enableNetDeviceEnumeration(false));
    ASSERT_NO_THROW(ctx_->enableNetDeviceEnumeration(true));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    ASSERT_GT(devList->getCount(), 0u) << "No devices enumerated after re-enabling network discovery";
    EXPECT_TRUE(hasNetDevice(devList)) << "Configured network device (ip=" << netIp << ") was not found after re-enabling network discovery";
}

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_03_net_device_direct) {
    const std::string &ip = ENV().netIp();
    if(ip.empty()) {
        GTEST_SKIP() << "network.ip not set in config";
    }

    {
        auto netDev = ctx_->createNetDevice(ip.c_str(), 8090);
        ASSERT_NE(netDev, nullptr);
        auto info = netDev->getDeviceInfo();
        ASSERT_NE(info, nullptr);
        EXPECT_NE(info->getName(), nullptr);
        EXPECT_NE(info->getSerialNumber(), nullptr);
        netDev.reset();
    }

    {
        auto netDev = ctx_->createNetDevice(ip.c_str(), 8090, OB_DEVICE_CONTROL_ACCESS);
        ASSERT_NE(netDev, nullptr);
        auto info = netDev->getDeviceInfo();
        ASSERT_NE(info, nullptr);
        EXPECT_NE(info->getName(), nullptr);
        EXPECT_NE(info->getSerialNumber(), nullptr);
        netDev.reset();
    }
}

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_04_force_ip) {
    if(!isDestructiveAllowed()) {
        GTEST_SKIP() << "Destructive tests not enabled in config";
    }
    ASSERT_TRUE(rebootAndReconnect(ctx_, cfg_.deviceSerial(), device_, devInfo_)) << "Device did not reconnect after reboot";
    const std::string &mac      = ENV().netMac();
    const std::string &ip       = ENV().netIp();
    const std::string &original = ENV().netOriginalIp();
    if(mac.empty() || ip.empty()) {
        GTEST_SKIP() << "network.mac and network.ip must be set in config";
    }

    std::string originalSn;
    if(!original.empty()) {
        try {
            auto origDev = ctx_->createNetDevice(original.c_str(), 8090);
            if(origDev) {
                auto origInfo = origDev->getDeviceInfo();
                if(origInfo && origInfo->getSerialNumber()) {
                    originalSn = origInfo->getSerialNumber();
                }
            }
        }
        catch(const std::exception &) {
        }
    }

    auto parseIPv4 = [](const char *str, uint8_t out[4]) -> bool {
        unsigned a, b, c, d;
        if(std::sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
            return false;
        if(a > 255 || b > 255 || c > 255 || d > 255)
            return false;
        out[0] = static_cast<uint8_t>(a);
        out[1] = static_cast<uint8_t>(b);
        out[2] = static_cast<uint8_t>(c);
        out[3] = static_cast<uint8_t>(d);
        return true;
    };

    OBNetIpConfig config{};
    config.dhcp = 0;
    ASSERT_TRUE(parseIPv4(ip.c_str(), config.address)) << "network.ip is not a valid IPv4 address: " << ip;

    const std::string &mask = ENV().netMask();
    if(!mask.empty()) {
        ASSERT_TRUE(parseIPv4(mask.c_str(), config.mask)) << "network.mask is not a valid IPv4 address: " << mask;
    }
    else {
        config.mask[0] = 255;
        config.mask[1] = 255;
        config.mask[2] = 255;
        config.mask[3] = 0;
    }

    const std::string &gw = ENV().netGateway();
    if(!gw.empty()) {
        ASSERT_TRUE(parseIPv4(gw.c_str(), config.gateway)) << "network.gateway is not a valid IPv4 address: " << gw;
    }
    else {
        config.gateway[0] = config.address[0];
        config.gateway[1] = config.address[1];
        config.gateway[2] = config.address[2];
        config.gateway[3] = 1;
    }

    bool ok = ctx_->forceIp(mac.c_str(), config);
    EXPECT_TRUE(ok) << "forceIp() returned false";

    std::this_thread::sleep_for(std::chrono::seconds(5));

    try {
        auto netDev = ctx_->createNetDevice(ip.c_str(), 8090);
        ASSERT_NE(netDev, nullptr) << "createNetDevice returned null after forceIp";
        auto info = netDev->getDeviceInfo();
        ASSERT_NE(info, nullptr);
        EXPECT_NE(info->getName(), nullptr);
        if(!originalSn.empty()) {
            EXPECT_STREQ(info->getSerialNumber(), originalSn.c_str())
                << "Serial number changed after forceIp - expected: " << originalSn << ", got: " << info->getSerialNumber();
        }
    }
    catch(const ob::Error &e) {
        GTEST_SKIP() << "Device unreachable on new IP " << ip << ": " << e.getMessage();
    }
}

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_05_hotplug_reboot) {
    if(!isDestructiveAllowed()) {
        GTEST_SKIP() << "Destructive tests not enabled in config";
    }

    auto waitForRebootReconnect = [&](const std::string &serial) -> bool {
        for(int elapsed = 0; elapsed < 60000; elapsed += 1000) {
            auto devList = ctx_->queryDeviceList();
            if(devList && devList->getCount() > 0) {
                for(uint32_t i = 0; i < devList->getCount(); i++) {
                    const char *sn = devList->getSerialNumber(i);
                    if(sn && serial == sn) {
                        return true;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return false;
    };

    std::atomic<bool> removedCalled{ false };
    std::atomic<bool> addedCalled{ false };

    auto cbId = ctx_->registerDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> removed, std::shared_ptr<ob::DeviceList> added) {
        if(removed && removed->getCount() > 0)
            removedCalled = true;
        if(added && added->getCount() > 0)
            addedCalled = true;
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto sn = std::string(devInfo_->getSerialNumber());
    device_->reboot();
    device_.reset();

    std::this_thread::sleep_for(std::chrono::seconds(5));
    EXPECT_TRUE(waitForRebootReconnect(sn)) << "Device did not re-enumerate within 60 s after first reboot";

    EXPECT_TRUE(removedCalled.load()) << "Device removed callback not received";
    EXPECT_TRUE(addedCalled.load()) << "Device added callback not received";

    ctx_->unregisterDeviceChangedCallback(cbId);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    removedCalled = false;
    addedCalled   = false;

    {
        auto devList = ctx_->queryDeviceList();
        ASSERT_TRUE(devList && devList->getCount() > 0);
        auto dev = devList->getDeviceBySN(sn.c_str());
        ASSERT_NE(dev, nullptr);
        device_ = dev;
        device_->reboot();
        device_.reset();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    EXPECT_TRUE(waitForRebootReconnect(sn)) << "Device did not re-enumerate within 60 s after second reboot";

    EXPECT_FALSE(removedCalled.load()) << "Callback triggered after unregister";
    EXPECT_FALSE(addedCalled.load()) << "Callback triggered after unregister";
}

TEST_F(TC_CPP_02_Discovery_HW, TC_CPP_02_06_clock_sync) {
    ClockSyncGuard guard(ctx_, 0);

    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(1000));

    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(500));

    ASSERT_NO_THROW(ctx_->enableDeviceClockSync(0));
}
