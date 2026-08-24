// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_03_DeviceList : public DeviceTest {};

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_01_count_and_index) {
    auto devList = ctx_->queryDeviceList();
    auto count   = devList->getCount();
    ASSERT_GT(count, 0u) << "No devices found";

    for(uint32_t i = 0; i < count; i++) {
        auto device = devList->getDevice(i);
        ASSERT_NE(device, nullptr) << "getDevice(" << i << ") returned null";

        auto info = device->getDeviceInfo();
        ASSERT_NE(info, nullptr);
        EXPECT_NE(devList->getName(i), nullptr);
        EXPECT_GT(devList->getPid(i), 0);
    }
}

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_02_access_mode) {
    auto sn = std::string(devInfo_->getSerialNumber());
    devInfo_.reset();
    device_.reset();

    auto devList = ctx_->queryDeviceList();

    {
        auto dev = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_EXCLUSIVE_ACCESS);
        ASSERT_NE(dev, nullptr);
        auto info = dev->getDeviceInfo();
        EXPECT_STREQ(info->getSerialNumber(), sn.c_str());
    }

    {
        auto dev = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_CONTROL_ACCESS);
        ASSERT_NE(dev, nullptr);
        auto info = dev->getDeviceInfo();
        EXPECT_STREQ(info->getSerialNumber(), sn.c_str());
    }

    {
        auto dev = devList->getDeviceBySN(sn.c_str(), OB_DEVICE_DEFAULT_ACCESS);
        ASSERT_NE(dev, nullptr);
    }
}

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_03_get_by_sn_uid) {
    auto sn  = std::string(devInfo_->getSerialNumber());
    auto uid = std::string(devInfo_->getUid());
    devInfo_.reset();
    device_.reset();

    auto devList = ctx_->queryDeviceList();

    {
        auto dev = devList->getDeviceBySN(sn.c_str());
        ASSERT_NE(dev, nullptr);
        auto info = dev->getDeviceInfo();
        EXPECT_STREQ(info->getSerialNumber(), sn.c_str());
    }

    {
        auto dev = devList->getDeviceByUid(uid.c_str());
        ASSERT_NE(dev, nullptr);
        auto info = dev->getDeviceInfo();
        EXPECT_STREQ(info->getUid(), uid.c_str());
    }

    {
        std::shared_ptr<ob::Device> dev;
        try {
            dev = devList->getDeviceBySN("NON_EXISTENT_SN_12345");
        }
        catch(const ob::Error &) {
        }
        EXPECT_EQ(dev, nullptr) << "Non-existent SN should return null device";
    }

    {
        std::shared_ptr<ob::Device> dev;
        try {
            dev = devList->getDeviceByUid("NON_EXISTENT_UID_12345");
        }
        catch(const ob::Error &) {
        }
        EXPECT_EQ(dev, nullptr) << "Non-existent UID should return null device";
    }
}

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_04_basic_info_fields) {
    auto devList = ctx_->queryDeviceList();
    for(uint32_t i = 0; i < devList->getCount(); i++) {
        EXPECT_NE(devList->getName(i), nullptr);
        EXPECT_GT(devList->getPid(i), 0);
        EXPECT_EQ(devList->getVid(i), 0x2BC5);
        EXPECT_NE(devList->getConnectionType(i), nullptr);
    }
}

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_05_net_device_info) {
    if(!hasNetworkConfig()) {
        GTEST_SKIP() << "network.ip and network.mac must be set in config";
    }

    auto isValidIPv4 = [](const char *ip) -> bool {
        if(!ip)
            return false;
        unsigned a, b, c, d;
        return std::sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255;
    };

    auto isValidMac = [](const char *mac) -> bool {
        if(!mac)
            return false;
        unsigned parts[6] = { 0 };
        int      matched  = std::sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x", &parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5]);
        return matched == 6;
    };

    auto devList = ctx_->queryDeviceList();
    bool found   = false;

    for(uint32_t i = 0; i < devList->getCount(); i++) {
        auto connType = std::string(devList->getConnectionType(i));
        if(connType == "Ethernet") {
            found = true;

            auto ip = devList->getIpAddress(i);
            EXPECT_NE(ip, nullptr);
            EXPECT_TRUE(isValidIPv4(ip)) << "Invalid IPv4 format: " << (ip ? ip : "null");
            EXPECT_STRNE(ip, "0.0.0.0");

            auto subnet = devList->getSubnetMask(i);
            EXPECT_NE(subnet, nullptr);
            EXPECT_TRUE(isValidIPv4(subnet)) << "Invalid subnet mask format: " << (subnet ? subnet : "null");

            auto gateway = devList->getGateway(i);
            EXPECT_NE(gateway, nullptr);
            EXPECT_TRUE(isValidIPv4(gateway)) << "Invalid gateway format: " << (gateway ? gateway : "null");

            auto localMac = devList->getLocalMacAddress(i);
            EXPECT_NE(localMac, nullptr);
            EXPECT_TRUE(isValidMac(localMac)) << "Invalid MAC format: " << (localMac ? localMac : "null");

            auto localIp = devList->getLocalIP(i);
            EXPECT_NE(localIp, nullptr);
            EXPECT_TRUE(isValidIPv4(localIp)) << "Invalid local IP format: " << (localIp ? localIp : "null");
        }
    }

    if(!found) {
        GTEST_SKIP() << "No Ethernet device found, skipping network info test";
    }
}

TEST_F(TC_CPP_03_DeviceList, TC_CPP_03_06_out_of_bounds) {
    auto devList = ctx_->queryDeviceList();
    auto count   = devList->getCount();
    ASSERT_GT(count, 0u);

    EXPECT_THROW(devList->getDevice(count), ob::Error);

    EXPECT_THROW(devList->getDevice(count + 100), ob::Error);

    EXPECT_THROW(devList->getDevice(UINT32_MAX), ob::Error);

    auto dev = devList->getDevice(0);
    EXPECT_NE(dev, nullptr);
}
