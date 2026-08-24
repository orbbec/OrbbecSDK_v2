// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_04_DeviceInfo : public DeviceTest {};

TEST_F(TC_CPP_04_DeviceInfo, TC_CPP_04_01_basic_info) {
    EXPECT_NE(devInfo_->getName(), nullptr);
    EXPECT_GT(std::strlen(devInfo_->getName()), 0u);
    EXPECT_NE(devInfo_->getSerialNumber(), nullptr);
    EXPECT_GT(std::strlen(devInfo_->getSerialNumber()), 0u);

    const char *fw = devInfo_->getFirmwareVersion();
    EXPECT_NE(fw, nullptr);
    EXPECT_GT(std::strlen(fw), 0u);
    {
        unsigned major, minor, patch;
        int      matched = std::sscanf(fw, "%u.%u.%u", &major, &minor, &patch);
        EXPECT_EQ(matched, 3) << "Firmware version format should be x.y.z: " << (fw ? fw : "null");
    }

    EXPECT_NE(devInfo_->getHardwareVersion(), nullptr);
    EXPECT_GT(std::strlen(devInfo_->getHardwareVersion()), 0u);
}

TEST_F(TC_CPP_04_DeviceInfo, TC_CPP_04_02_id_fields) {
    EXPECT_GT(devInfo_->getPid(), 0);
    EXPECT_EQ(devInfo_->getVid(), 0x2BC5);
    EXPECT_NE(devInfo_->getUid(), nullptr);
    EXPECT_GT(std::strlen(devInfo_->getUid()), 0u);
    auto conn = devInfo_->getConnectionType();
    EXPECT_NE(conn, nullptr);
}

TEST_F(TC_CPP_04_DeviceInfo, TC_CPP_04_03_net_ip_info) {
    auto isValidIPv4 = [](const char *ip) -> bool {
        if(!ip)
            return false;
        unsigned a, b, c, d;
        return std::sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255;
    };

    if(!hasNetworkConfig()) {
        GTEST_SKIP() << "network.ip and network.mac must be set in config";
    }

    auto netDev = getNetDevice();
    ASSERT_NE(netDev, nullptr) << "Network device not reachable (ip=" << ENV().netIp() << ")";
    auto netInfo = netDev->getDeviceInfo();
    ASSERT_NE(netInfo, nullptr);

    auto ip = netInfo->getIpAddress();
    ASSERT_NE(ip, nullptr);
    EXPECT_TRUE(isValidIPv4(ip)) << "Invalid IPv4 format: " << (ip ? ip : "null");

    auto mask = netInfo->getDeviceSubnetMask();
    EXPECT_NE(mask, nullptr);
    EXPECT_TRUE(isValidIPv4(mask)) << "Invalid subnet mask format: " << (mask ? mask : "null");

    auto gw = netInfo->getDeviceGateway();
    EXPECT_NE(gw, nullptr);
    EXPECT_TRUE(isValidIPv4(gw)) << "Invalid gateway format: " << (gw ? gw : "null");
}

TEST_F(TC_CPP_04_DeviceInfo, TC_CPP_04_04_chip_type_info) {
    auto asic = devInfo_->getAsicName();
    EXPECT_NE(asic, nullptr);
    auto devType = devInfo_->getDeviceType();
    (void)devType;
    auto minSdk = devInfo_->getSupportedMinSdkVersion();
    EXPECT_NE(minSdk, nullptr);
}

TEST_F(TC_CPP_04_DeviceInfo, TC_CPP_04_05_extension_info) {
    bool exists = device_->isExtensionInfoExist("SerialNumber");
    if(exists) {
        auto val = device_->getExtensionInfo("SerialNumber");
        EXPECT_NE(val, nullptr) << "Extension info value should not be null for existing key";
    }
    EXPECT_FALSE(device_->isExtensionInfoExist("TotallyBogusExtKey_12345"));
}
