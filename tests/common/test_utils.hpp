// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace test_utils {

inline bool tryGetDeviceState(const std::string &sn, int &pid, std::string &type) {
    try {
        auto ctx = std::make_shared<ob::Context>();
        if(!ctx)
            return false;

        auto devList = ctx->queryDeviceList();
        if(!devList || devList->deviceCount() == 0)
            return false;

        std::shared_ptr<ob::Device> dev;
        if(!sn.empty()) {
            dev = devList->getDeviceBySN(sn.c_str());
        }
        else {
            dev = devList->getDevice(0);
        }
        if(!dev)
            return false;

        auto info = dev->getDeviceInfo();
        if(!info)
            return false;

        pid  = info->getPid();
        type = info->getName() ? std::string(info->getName()) : "unknown";
        return true;
    }
    catch(...) {
        return false;
    }
}

inline std::string pidToHex(int pid) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%04x", pid);
    return std::string(buf);
}

inline bool fileExists(const std::string &path) {
    if(path.empty())
        return false;
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

inline std::string baseName(const std::string &path) {
    if(path.empty())
        return "";
    auto pos = path.find_last_of("\\/");
    if(pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

inline std::string readFileContent(const std::string &path) {
    if(path.empty())
        return "";
    std::ifstream f(path, std::ios::binary);
    if(!f)
        return "";
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

inline std::string extractConfigPath(int &argc, char **argv) {
    std::string configPath;
    int         outIndex = 1;
    for(int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if(arg.compare(0, 9, "--config=") == 0) {
            configPath = arg.substr(9);
        }
        else {
            argv[outIndex] = argv[i];
            ++outIndex;
        }
    }
    argc = outIndex;
    return configPath;
}

inline void setupTestLogger() {
    const char *logDir = std::getenv("OB_TEST_LOG_DIR");
    if(logDir && logDir[0]) {
        ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, logDir);
    }
}

inline std::shared_ptr<ob::FrameSet> waitForFramesetWithRetry(std::shared_ptr<ob::Pipeline> &pipeline, int retries = 15) {
    for(int i = 0; i < retries; ++i) {
        auto fs = pipeline->waitForFrameset();
        if(fs)
            return fs;
    }
    return nullptr;
}

inline bool hasSensorType(const std::shared_ptr<ob::SensorList> &sensorList, OBSensorType type) {
    if(!sensorList)
        return false;
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        if(sensorList->getSensorType(i) == type)
            return true;
    }
    return false;
}

inline std::shared_ptr<ob::Sensor> tryGetSensor(const std::shared_ptr<ob::Device> &device, OBSensorType type) {
    if(!device)
        return nullptr;
    auto sensorList = device->getSensorList();
    if(!hasSensorType(sensorList, type))
        return nullptr;
    try {
        return device->getSensor(type);
    }
    catch(const ob::Error &) {
        return nullptr;
    }
}

inline std::shared_ptr<ob::VideoStreamProfile> getFirstVideoProfile(const std::shared_ptr<ob::SensorList> &sensorList, OBSensorType targetType) {
    if(!sensorList)
        return nullptr;
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        if(sensorList->getSensorType(i) != targetType)
            continue;
        auto sensor = sensorList->getSensor(i);
        if(!sensor)
            continue;
        auto profiles = sensor->getStreamProfileList();
        if(!profiles || profiles->getCount() == 0)
            continue;
        for(uint32_t j = 0; j < profiles->getCount(); ++j) {
            auto profile = profiles->getProfile(j);
            if(profile && profile->is<ob::VideoStreamProfile>()) {
                return profile->as<ob::VideoStreamProfile>();
            }
        }
    }
    return nullptr;
}

}  // namespace test_utils
