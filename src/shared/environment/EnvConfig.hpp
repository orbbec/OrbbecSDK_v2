// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "xml/XmlReader.hpp"
#include "libobsensor/h/ObTypes.h"
#include <string>

namespace libobsensor {

class EnvConfig {

    explicit EnvConfig(const std::string &configFilePath);

    static std::mutex               instanceMutex_;
    static std::weak_ptr<EnvConfig> instanceWeakPtr_;

    static std::string extensionsDir_;
    static std::string currentWorkDir_;

public:
    static std::shared_ptr<EnvConfig> getInstance(const std::string &configFilePath = "");

    ~EnvConfig() noexcept = default;

    bool isNodeContained(const std::string &nodePathName);
    bool getIntValue(const std::string &nodePathName, int &t);
    bool getBooleanValue(const std::string &nodePathName, bool &t);
    bool getFloatValue(const std::string &nodePathName, float &t);
    bool getDoubleValue(const std::string &nodePathName, double &t);
    bool getStringValue(const std::string &nodePathName, std::string &t);

    static const std::string &getExtensionsDirectory();
    static void               setExtensionsDirectory(const std::string &dir);

private:
    std::vector<std::shared_ptr<XmlReader>> xmlReaders_;

#ifdef __ANDROID__
    std::string defaultConfigFile_ = "/sdcard/orbbec/OrbbecSDKConfig.xml";
#else
    std::string defaultConfigFile_ = "OrbbecSDKConfig.xml";
#endif
};

class DevInfoConfig {

    explicit DevInfoConfig();

    static std::mutex                   instanceMutex_;
    static std::weak_ptr<DevInfoConfig> instanceWeakPtr_;

public:
    static std::shared_ptr<DevInfoConfig> getInstance();

    ~DevInfoConfig() noexcept = default;

    bool isNodeContained(const std::string &nodePathName);
    bool getStringValue(const std::string &nodePathName, std::string &t);
    bool getChildNodeNames(const std::string &nodePathName, std::vector<std::string> &childNames);
    bool getChildNodeTextList(const std::string &nodePathName, std::vector<std::string> &texts);
    bool getAttributeValue(const std::string &nodePathName, const std::string &attrName, std::string &value);

private:
    std::vector<std::shared_ptr<XmlReader>> xmlReaders_;
};

}  // namespace libobsensor
