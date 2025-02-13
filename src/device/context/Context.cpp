// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Context.hpp"
#include "utils/Utils.hpp"
#include "devicemanager/DeviceManager.hpp"
#include "environment/Version.hpp"

#include <mutex>

namespace libobsensor {

std::mutex             Context::instanceMutex_;
std::weak_ptr<Context> Context::instanceWeakPtr_;

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
    filterFactory_           = FilterFactory::getInstance();
    platform_                = Platform::getInstance();

    if(configFilePath.empty()) {
        LOG_DEBUG("Context created! Library version: v{}", OB_LIB_VERSION_STR);
    }
    else {
        LOG_DEBUG("Context created! Library version: v{}, config file path: {}", OB_LIB_VERSION_STR, configFilePath);
    }
#ifdef OB_BUILD_WITH_EXTENSIONS_COMMIT_HASH
    logExtensionsCommitHashes();
#endif
}

Context::~Context() noexcept {}

std::shared_ptr<IDeviceManager> Context::getDeviceManager() {
    if(deviceManager_ == nullptr) {
        deviceManager_ = DeviceManager::getInstance();
    }
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

#ifdef OB_BUILD_WITH_EXTENSIONS_COMMIT_HASH
typedef const char *(*pfunc_ob_get_commit_hash)();

void Context::logExtensionsCommitHashes() {
    std::unordered_map<std::string, std::pair<std::string, std::string>> extensionsMap = {
        { "frameprocessor", { "/frameprocessor/", "ob_frame_processor" } },
        { "privfilter", { "/filters/", "ob_priv_filter" } },
        { "filterprocessor", { "/filters/", "FilterProcessor" } },
        { "firmwareupdater", { "/firmwareupdater/", "firmwareupdater" } },
    };

    for(const auto &libInfo: extensionsMap) {
        try {
            std::string              moduleLoadPath     = EnvConfig::getExtensionsDirectory() + libInfo.second.first;
            auto                     dylib_             = std::make_shared<dylib>(moduleLoadPath.c_str(), libInfo.second.second.c_str());
            pfunc_ob_get_commit_hash ob_get_commit_hash = dylib_->get_function<const char *()>("ob_get_commit_hash");

            if(dylib_ && ob_get_commit_hash) {
                const char *commitHash = ob_get_commit_hash();
                LOG_DEBUG(" - Successfully retrieved commit hash for library '{}' (commit: {})", libInfo.first, commitHash);
            }
            else {
                LOG_DEBUG(" - Failed to retrieve commit hash for library '{}'", libInfo.first);
            }
        }
        catch(...) {
            LOG_DEBUG(" - Failed to retrieve commit hash for library '{}', exception occurred", libInfo.first);
        }
    }
}
#endif

}  // namespace libobsensor
