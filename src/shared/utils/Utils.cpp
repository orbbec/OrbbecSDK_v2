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
#include "logger/LoggerInterval.hpp"

namespace libobsensor {
namespace utils {

uint64_t getNowTimesMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t getNowTimesUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint64_t getSteadyTimeMs() {
#if defined(__linux__) || defined(__ANDROID__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000u + static_cast<uint64_t>(ts.tv_nsec) / 1000000u;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

uint64_t getSteadyTimeUs() {
#if defined(__linux__) || defined(__ANDROID__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000u + static_cast<uint64_t>(ts.tv_nsec) / 1000u;
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

HostTimestamp getHostTimestampUs() {
    const auto steadyBefore = getSteadyTimeUs();
    const auto systemTime   = getNowTimesUs();
    const auto steadyAfter  = getSteadyTimeUs();
    const auto steadySpan   = steadyAfter - steadyBefore;

    constexpr uint64_t retryThresholdUs = 100;
    if(steadySpan <= retryThresholdUs) {
        return { systemTime, steadyBefore + steadySpan / 2 };
    }

    // Reuse the first bracket's endpoint as the second bracket's start.
    const auto    retrySystem = getNowTimesUs();
    const auto    retryAfter  = getSteadyTimeUs();
    const auto    retrySpan   = retryAfter - steadyAfter;
    HostTimestamp timestamp{ systemTime, steadyBefore + steadySpan / 2 };
    bool          projected = false;
    // Use the tighter retry bracket to estimate the clock offset, then project it
    // back to the earlier systemTime so both returned values represent that time.
    if(retrySpan < steadySpan) {
        const auto retrySteadyMid = steadyAfter + retrySpan / 2;
        if(retrySystem >= systemTime) {
            const auto systemTimeDiff = retrySystem - systemTime;
            if(retrySteadyMid >= systemTimeDiff) {
                const auto estimatedSteady = retrySteadyMid - systemTimeDiff;
                // Under a stable wall clock, systemTime was read between
                // steadyBefore and steadyAfter. Allow for microsecond rounding.
                constexpr uint64_t roundingToleranceUs = 2;
                const auto         lowerBound          = steadyBefore > roundingToleranceUs ? steadyBefore - roundingToleranceUs : 0;
                const auto         upperBound          = steadyAfter + roundingToleranceUs;
                if(estimatedSteady >= lowerBound && estimatedSteady <= upperBound) {
                    timestamp.steadyTimeUs = estimatedSteady;
                    projected              = true;
                }
            }
        }
    }
    LOG_INTVL("getHostTimestampUs.retry", 10000, spdlog::level::debug,
              "Host timestamp retry: steadySpan1={}us, steadySpan2={}us, systemTimeDiff={}us, selected={}, systemTime={}us, steadyTime={}us, offset={}us",
              steadySpan, retrySpan, static_cast<int64_t>(retrySystem) - static_cast<int64_t>(systemTime), projected ? "projected" : "first_midpoint",
              timestamp.systemTimeUs, timestamp.steadyTimeUs, static_cast<int64_t>(timestamp.systemTimeUs) - static_cast<int64_t>(timestamp.steadyTimeUs));
    return timestamp;
}

void sleepMs(uint64_t msec) {
#ifdef WIN32
    Sleep((DWORD)msec);
#else
    usleep((useconds_t)(msec * 1000));
#endif
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
