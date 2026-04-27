// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Utils.hpp"
#include "common/DeviceSeriesInfo.hpp"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <chrono>
#include <logger/Logger.hpp>

namespace libobsensor {
namespace utils {

uint64_t getNowTimesMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t getNowTimesUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t getSteadyTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint64_t getSteadyTimeUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void sleepMs(uint64_t msec) {
#ifdef WIN32
    Sleep((DWORD)msec);
#else
    usleep((useconds_t)(msec * 1000));
#endif
}

bool checkJpgImageData(const uint8_t *data, size_t dataLen) {
    bool validImage = dataLen >= 2 && data[0] == 0xFF && data[1] == 0xD8;
    if(validImage) {
        // To resolve misjudgments caused by the MJPG data tail (0xFF 0xD9) not being arranged according to the standard MJPG format due to alignment and
        // other issues
        size_t startIndex = dataLen - 1;
        size_t endIndex   = startIndex - 10;
        for(size_t index = startIndex; index > endIndex && index > 0; index--) {
            if(data[index] != 0x00) {
                if(data[index] == 0xD9 && data[index - 1] == 0xFF) {
                    validImage = true;
                }
                else {
                    LOG_DEBUG("check mjpg end flag failed:data[index]:{0},data[index - 1]:{1}", data[index], data[index - 1]);
                    validImage = false;
                }
                break;
            }
        }
    }
    return validImage;
}

int findJpgSequence(const uint8_t *data, uint32_t size, uint32_t startIndex, const uint8_t *target, uint32_t targetLength) {
    // Limit the maximum search length to 1024 bytes for performance and safety
    if(size - startIndex > 1024) {
        size = 1024 + startIndex;
    }

    // Return -1 early if data is too small to contain the target sequence
    if(size < targetLength || startIndex > size - targetLength) {
        return -1;
    }

    const uint32_t maxIndex = size - targetLength;
    for(uint32_t i = startIndex; i <= maxIndex; ++i) {
        bool matched = true;
        for(uint32_t j = 0; j < targetLength; ++j) {
            if(data[i + j] != target[j]) {
                matched = false;
                break;
            }
        }
        if(matched) {
            return i;  // Now returning size_t to match function signature
        }
    }

    return -1;
}

int findJpgSOSSequence(const uint8_t *data, uint32_t size, uint32_t startIndex) {
    constexpr uint8_t kSOSSequence[] = { 0xFF, 0xDA };
    return findJpgSequence(data, size, startIndex, kSOSSequence, sizeof(kSOSSequence));
}

int findJpgCOMSequence(const uint8_t *data, uint32_t size, uint32_t startIndex) {
    constexpr uint8_t kCOMSequence[] = { 0xFF, 0xFE };
    return findJpgSequence(data, size, startIndex, kCOMSequence, sizeof(kCOMSequence));
}

int getJpgHeadLength(const uint8_t *data, uint32_t size) {
    const uint32_t sosSequencefixedDistance = 14;
    int            sosSequenceIndex         = findJpgSOSSequence(data, size);
    if(sosSequenceIndex == -1) {
        return -1;
    }

    uint32_t jpegHeadSize = sosSequenceIndex + sosSequencefixedDistance;
    if(jpegHeadSize >= size) {
        return -1;
    }
    return jpegHeadSize;
}

bool validateIpConfig(uint32_t ip, uint32_t mask, uint32_t gateway, bool allowZeroGateWay) {
    // ip
    if(ip == 0x00000000 || ip == 0xFFFFFFFF || ((ip & 0xF0000000) == 0xE0000000) || ((ip & 0xF0000000) == 0xF0000000) || ((ip & 0xFF) == 0x00)
       || ((ip & 0xFF) == 0xFF)) {
        return false;
    }

    // subnet mask
    if(mask == 0 || mask == 0xFFFFFFFF || ((~mask) & ((~mask) + 1)) != 0) {
        return false;
    }

    // If allowZeroGateWay is true and gateway is 0, we skip other checks (gateway validation)
    if(gateway == 0 && allowZeroGateWay) {
        return true;
    }

    // gateway
    if(gateway == 0x00000000 || gateway == 0xFFFFFFFF || ((gateway & 0xF0000000) == 0xE0000000) || ((gateway & 0xF0000000) == 0xF0000000)
       || ((gateway & 0xFF) == 0x00) || ((gateway & 0xFF) == 0xFF)) {
        return false;
    }
    // Ensure that the IP and gateway are in the same subnet
    if((ip & mask) != (gateway & mask)) {
        return false;
    }

    return true;
}

bool checkIpConfig(const ob_net_ip_config &config, bool allowZeroGateWay) {
    uint32_t ip      = (config.address[3]) | (config.address[2] << 8) | (config.address[1] << 16) | (config.address[0] << 24);
    uint32_t mask    = (config.mask[3]) | (config.mask[2] << 8) | (config.mask[1] << 16) | (config.mask[0] << 24);
    uint32_t gateway = (config.gateway[3]) | (config.gateway[2] << 8) | (config.gateway[1] << 16) | (config.gateway[0] << 24);

    if(config.dhcp != 0) {
        // Skip validation if DHCP is enabled
        return true;
    }

    return validateIpConfig(ip, mask, gateway, allowZeroGateWay);
}

bool checkIpConfig(const ob_net_ip_config_v2 &config, bool allowZeroGateWay) {
    uint32_t ip      = (config.address[3]) | (config.address[2] << 8) | (config.address[1] << 16) | (config.address[0] << 24);
    uint32_t mask    = (config.mask[3]) | (config.mask[2] << 8) | (config.mask[1] << 16) | (config.mask[0] << 24);
    uint32_t gateway = (config.gateway[3]) | (config.gateway[2] << 8) | (config.gateway[1] << 16) | (config.gateway[0] << 24);

    if((config.flags & OB_NET_IP_FLAG_PERSISTENT) == 0) {
        // Skip validation if persistent IP is not enabled
        return true;
    }

    return validateIpConfig(ip, mask, gateway, allowZeroGateWay);
}

std::string getSDKLibraryName() {
#ifdef OB_SDK_LIBRARY_NAME
    std::string sdkLibraryName = OB_SDK_LIBRARY_NAME;
    if(!sdkLibraryName.empty()) {
        return sdkLibraryName;
    }
#endif
    return "OrbbecSDK";
}

OBIpSourceType parseGevCurIpConfig(const uint32_t &rawConfigSet) {
    constexpr uint32_t bitPersistentIp = 0x01;
    if(rawConfigSet & bitPersistentIp) {
        return OB_IP_SOURCE_PERSISTENT;
    }

    constexpr uint32_t bitDHCP = 0x02;
    if(rawConfigSet & bitDHCP) {
        return OB_IP_SOURCE_DHCP;
    }

    constexpr uint32_t bitLLA = 0x04;
    if(rawConfigSet & bitLLA) {
        return OB_IP_SOURCE_LLA;
    }

    return OB_IP_SOURCE_NONE;
}

bool isAllowZeroGateway(uint32_t vid, uint32_t pid) {
    return isDeviceInContainer(G335LeDevPids, vid, pid) || isDeviceInContainer(G435LeDevPids, vid, pid);
}

bool isSameSubnet(const std::string &localIp, const std::string &devIp, uint8_t subnetLength) {
    uint32_t a = inet_addr(localIp.c_str());
    uint32_t b = inet_addr(devIp.c_str());
    if(subnetLength >= 32) {
        return a == b;
    }

    uint32_t mask = htonl(~((1U << (32 - subnetLength)) - 1));
    return (a & mask) == (b & mask);
}

}  // namespace utils
}  // namespace libobsensor
