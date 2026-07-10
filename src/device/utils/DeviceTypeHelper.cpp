// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceTypeHelper.hpp"

namespace libobsensor {

namespace {
constexpr uint32_t HOST_PLATFORM_OS_TYPE_MASK = 0xFFu;
}

std::ostream &operator<<(std::ostream &os, const libobsensor::DeviceComponentId &id) {
    os << "Device Component '";
    auto iter = libobsensor::DeviceComponentIdMap.find(id);
    if(iter == libobsensor::DeviceComponentIdMap.end()) {
        os << "unknown'";
    }
    else {
        os << iter->second << "'";
    }
    return os;
}

uint32_t getHostPlatformType() {
    uint32_t hostPlatform = 0x00;
#if defined(OS_WIN) || defined(_WIN32) || defined(WIN32) || defined(WINCE)
    hostPlatform = 0x01;  // Windows
#elif defined(BUILD_ANDROID) || defined(__ANDROID__)
    hostPlatform = 0x04;  // Android
#elif defined(OS_IOS)
    hostPlatform = 0x05;  // iOS
#elif defined(OS_MACOS) || defined(OS_APPLE) || defined(OS_MACOS_X64) || defined(OS_MACOS_ARM64) || defined(__APPLE__)
    hostPlatform = 0x03;  // macOS
#elif defined(__OHOS__) || defined(OHOS) || defined(HARMONYOS)
    hostPlatform = 0x06;  // HarmonyOS
#elif defined(OS_LINUX) || defined(__linux__)
    hostPlatform = 0x02;  // Linux
#endif

    return hostPlatform & HOST_PLATFORM_OS_TYPE_MASK;
}

}  // namespace libobsensor
