// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EnvConfig.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(ob);

namespace libobsensor {

std::mutex               EnvConfig::instanceMutex_;
std::weak_ptr<EnvConfig> EnvConfig::instanceWeakPtr_;

std::shared_ptr<EnvConfig> EnvConfig::getInstance(const std::string &configFilePath) {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         ctxInstance = instanceWeakPtr_.lock();
    if(!ctxInstance) {
        ctxInstance      = std::shared_ptr<EnvConfig>(new EnvConfig(configFilePath));
        instanceWeakPtr_ = ctxInstance;
    }
    return ctxInstance;
}

EnvConfig::EnvConfig(const std::string &configFile) {
    // add external config file
    auto extConfigFile = configFile;
    if(extConfigFile.empty()) {
#ifndef __ANDROID__
        auto currentWorkDir = utils::getCurrentWorkDirectory();
        if(currentWorkDir == "") {
            currentWorkDir = "./";
        }
        extConfigFile = utils::joinPaths(currentWorkDir, defaultConfigFile_);
#else
        extConfigFile = defaultConfigFile_;
#endif
    }
    if(utils::fileExists(extConfigFile.c_str())) {
        TRY_EXECUTE({
            auto xmlReader = std::make_shared<XmlReader>(extConfigFile);
            xmlReaders_.push_back(xmlReader);
        });
    }

    // add default config file (binary resource compiled into the library via [cmrc](https://github.com/vector-of-bool/cmrc))
    auto fs           = cmrc::ob::get_filesystem();
    auto def_xml      = fs.open("config/OrbbecSDKConfig.xml");
    auto defXmlReader = std::make_shared<XmlReader>(def_xml.begin(), def_xml.size());
    xmlReaders_.push_back(defXmlReader);
}

bool EnvConfig::isNodeContained(const std::string &nodePathName) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->isNodeContained(nodePathName)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getIntValue(const std::string &nodePathName, int &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getIntValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getBooleanValue(const std::string &nodePathName, bool &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getBooleanValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getFloatValue(const std::string &nodePathName, float &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getFloatValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getDoubleValue(const std::string &nodePathName, double &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getDoubleValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getStringValue(const std::string &nodePathName, std::string &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getStringValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

std::string EnvConfig::extensionsDir_ = "extensions";
std::string EnvConfig::currentWorkDir_ = "./";

const std::string& EnvConfig::getExtensionsDirectory() {
    static std::string cachedPath = []() {
        auto extensionPath = currentWorkDir_ + extensionsDir_;
        // when the extension path is empty, use the current working directory
        if (extensionPath == "./extensions" || extensionPath == "./") {
            auto currentWorkDir = utils::getCurrentWorkDirectory();
            if (!currentWorkDir.empty()) {
                extensionsDir_ = extensionsDir_.empty() ? "extensions" : extensionsDir_;
                return utils::joinPaths(currentWorkDir, extensionsDir_);
            }
        }
        
        return extensionsDir_;
    }();
    
    return cachedPath;
}

void EnvConfig::setExtensionsDirectory(const std::string &dir) {
    extensionsDir_ = dir;
}

}  // namespace libobsensor
