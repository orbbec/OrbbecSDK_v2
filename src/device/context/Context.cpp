// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Context.hpp"
#include "devicemanager/DeviceManager.hpp"
#include "environment/Version.hpp"
#include "DynamicLibraryHelper.hpp"

#include <memory>
#include <mutex>

namespace libobsensor {

std::mutex             Context::instanceMutex_;
std::weak_ptr<Context> Context::instanceWeakPtr_;

/**
 * @brief Returns a shared pointer to the global Context instance.
 *
 * @param[in] configPath Config file path. Used only on the first call when the Context is created.
 *
 * @note IMPORTANT:
 *       1. Do NOT store this shared_ptr for long-term use.
 *       2. Always acquire the instance when needed and release it immediately.
 *       3. If a long-term reference is required (e.g., in a member variable), store a std::weak_ptr
 *          instead of std::shared_ptr to avoid potential lifetime and ownership issues.
 */
std::shared_ptr<Context> Context::getInstance(const std::string &configPath) {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         ctxInstance = instanceWeakPtr_.lock();
    if(!ctxInstance) {
        ctxInstance      = std::shared_ptr<Context>(new Context(configPath));
        instanceWeakPtr_ = ctxInstance;
    }
    return ctxInstance;
}

Context::Context(const std::string &configFilePath) {
    envConfig_               = EnvConfig::getInstance(configFilePath);
    logger_                  = Logger::getInstance();
    frameMemoryPool_         = FrameMemoryPool::getInstance();
    streamIntrinsicsManager_ = StreamIntrinsicsManager::getInstance();
    streamExtrinsicsManager_ = StreamExtrinsicsManager::getInstance();
    deviceSeriesInfoManager_ = DeviceSeriesInfoManager::getInstance();
    dynamicLibraryManager_   = DynamicLibraryManager::getInstance();
    filterFactory_           = FilterFactory::getInstance();
    platform_                = Platform::getInstance();
    hostTimestampProvider_   = std::make_shared<HostTimestampProvider>();

    if(configFilePath.empty()) {
        LOG_DEBUG("Context created! Library version: v{}", OB_LIB_VERSION_STR);
    }
    else {
        LOG_DEBUG("Context created! Library version: v{}, config file path: {}", OB_LIB_VERSION_STR, configFilePath);
    }
    registerDynamicLibraryLoadedCallbacks();
}

Context::~Context() noexcept {}

std::shared_ptr<IDeviceManager> Context::tryGetDeviceManager() {
    return deviceManager_;
}

std::shared_ptr<IDeviceManager> Context::getDeviceManager() {
    std::call_once(devMgrFlag_, [this]() {
        bool enumerateNetDevice = true;
        if(envConfig_) {
            envConfig_->getBooleanValue("Device.EnumerateNetDevice", enumerateNetDevice);
        }

        deviceManager_ = DeviceManager::getInstance();

        if(deviceManager_) {
            deviceManager_->enableNetDeviceEnumeration(enumerateNetDevice);
        }
    });

    return deviceManager_;
}

std::shared_ptr<Logger> Context::getLogger() const {
    return logger_;
}

std::shared_ptr<FrameMemoryPool> Context::getFrameMemoryPool() const {
    return frameMemoryPool_;
}

std::shared_ptr<Platform> Context::getPlatform() const {
    return platform_;
}

std::shared_ptr<HostTimestampProvider> Context::getHostTimestampProvider() const {
    return hostTimestampProvider_;
}

void Context::registerDynamicLibraryLoadedCallbacks() {
    dynamicLibraryManager_->registerLoadedCallback("firmwareupdater", [](const std::string &libraryName, const std::shared_ptr<dylib> &lib) {
        DynamicLibraryHelper::setExtensionDeviceInfo(libraryName, lib);
    });

    dynamicLibraryManager_->registerLoadedCallback("ob_frame_processor", [](const std::string &libraryName, const std::shared_ptr<dylib> &lib) {
        DynamicLibraryHelper::setExtensionDeviceInfo(libraryName, lib);
    });
}

}  // namespace libobsensor
