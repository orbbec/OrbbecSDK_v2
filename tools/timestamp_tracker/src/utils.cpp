// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "utils.hpp"
#include <libobsensor/hpp/TypeHelper.hpp>
#include <cctype>
#include <chrono>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace libobsensor {
namespace tools {

namespace {

std::string normalizeToken(const std::string &value) {
    std::string normalized;
    normalized.reserve(value.size());
    for(auto c: value) {
        if(std::isalnum(static_cast<unsigned char>(c))) {
            normalized.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
        }
    }
    return normalized;
}

bool matchesEnumToken(const std::string &input, const std::string &candidate, const std::string &suffix = "") {
    const auto normalizedInput     = normalizeToken(input);
    const auto normalizedCandidate = normalizeToken(candidate);
    if(normalizedInput == normalizedCandidate) {
        return true;
    }
    if(!suffix.empty() && normalizedCandidate.size() > suffix.size() && normalizedCandidate.substr(normalizedCandidate.size() - suffix.size()) == suffix) {
        return normalizedInput == normalizedCandidate.substr(0, normalizedCandidate.size() - suffix.size());
    }
    return false;
}

}  // namespace

std::string toLower(const std::string &s) {
    std::string out = s;
    for(auto &c: out) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool isImuSensorName(const std::string &name) {
    return normalizeToken(name) == "imu";
}

bool isWildcardToken(const std::string &value) {
    const auto normalized = normalizeToken(value);
    return normalized.empty() || normalized == "any" || normalized == "unknown";
}

// Sensor name string (case-insensitive) -> OBSensorType
// Uses SDK's own convertOBSensorTypeToString as the source of truth.
OBSensorType sensorNameToType(const std::string &name) {
    const std::string lowerName = toLower(name);
    for(int i = 0; i < static_cast<int>(OB_SENSOR_TYPE_COUNT); ++i) {
        auto type = static_cast<OBSensorType>(i);
        if(toLower(ob::TypeHelper::convertOBSensorTypeToString(type)) == lowerName) {
            return type;
        }
    }
    return OB_SENSOR_UNKNOWN;
}

// Format string (case-insensitive) -> OBFormat
// Uses SDK's own convertOBFormatTypeToString as the source of truth.
OBFormat formatStringToOBFormat(const std::string &fmt) {
    if(fmt.empty()) {
        return OB_FORMAT_UNKNOWN;
    }
    const std::string lowerFmt = toLower(fmt);
    for(int i = 0; i <= static_cast<int>(OB_FORMAT_LIDAR_CALIBRATION); ++i) {
        auto f = static_cast<OBFormat>(i);
        if(toLower(ob::TypeHelper::convertOBFormatTypeToString(f)) == lowerFmt) {
            return f;
        }
    }
    return OB_FORMAT_UNKNOWN;
}

OBAccelFullScaleRange accelFullScaleRangeFromString(const std::string &range) {
    if(isWildcardToken(range)) {
        return OB_ACCEL_FULL_SCALE_RANGE_ANY;
    }

    static const OBAccelFullScaleRange candidates[] = {
        OB_ACCEL_FS_2g, OB_ACCEL_FS_4g, OB_ACCEL_FS_8g, OB_ACCEL_FS_16g, OB_ACCEL_FS_3g, OB_ACCEL_FS_6g, OB_ACCEL_FS_12g, OB_ACCEL_FS_24g,
    };

    for(auto candidate: candidates) {
        if(matchesEnumToken(range, ob::TypeHelper::convertOBAccelFullScaleRangeTypeToString(candidate), "g")) {
            return candidate;
        }
    }

    return OB_ACCEL_FS_UNKNOWN;
}

OBGyroFullScaleRange gyroFullScaleRangeFromString(const std::string &range) {
    if(isWildcardToken(range)) {
        return OB_GYRO_FULL_SCALE_RANGE_ANY;
    }

    static const OBGyroFullScaleRange candidates[] = {
        OB_GYRO_FS_16dps,  OB_GYRO_FS_31dps,  OB_GYRO_FS_62dps,  OB_GYRO_FS_125dps,  OB_GYRO_FS_250dps,
        OB_GYRO_FS_500dps, OB_GYRO_FS_400dps, OB_GYRO_FS_800dps, OB_GYRO_FS_1000dps, OB_GYRO_FS_2000dps,
    };

    for(auto candidate: candidates) {
        if(matchesEnumToken(range, ob::TypeHelper::convertOBGyroFullScaleRangeTypeToString(candidate), "dps")) {
            return candidate;
        }
    }

    return OB_GYRO_FS_UNKNOWN;
}

// Check if a given sensor type is present on the device.
bool deviceHasSensor(std::shared_ptr<ob::Device> device, OBSensorType type) {
    auto sensorList = device->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        if(sensorList->getSensorType(i) == type) {
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
bool isEscPressed() {
    while(_kbhit()) {
        if(_getch() == 27) {
            return true;
        }
    }
    return false;
}
#else
bool isEscPressed() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
    newt.c_cc[VMIN]  = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char ch      = 0;
    bool pressed = (read(STDIN_FILENO, &ch, 1) == 1 && ch == 27);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return pressed;
}
#endif

uint64_t getWallTimesUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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

}  // namespace tools
}  // namespace libobsensor
