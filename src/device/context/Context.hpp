// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <memory>

#include "IDeviceManager.hpp"
#include "logger/Logger.hpp"
#include "environment/EnvConfig.hpp"
#include "frame/FrameMemoryPool.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "FilterFactory.hpp"
#include "Platform.hpp"

namespace libobsensor {
class Context {
private:
    explicit Context(const std::string &configFilePath = "");

    static std::mutex             instanceMutex_;
    static std::weak_ptr<Context> instanceWeakPtr_;

public:
    ~Context() noexcept;

    static std::shared_ptr<Context> getInstance(const std::string &configPath = "");

    std::shared_ptr<IDeviceManager>  getDeviceManager();
    std::shared_ptr<Logger>          getLogger() const;
    std::shared_ptr<FrameMemoryPool> getFrameMemoryPool() const;
    std::shared_ptr<Platform>        getPlatform() const;

private:
#ifdef OB_BUILD_WITH_EXTENSIONS_COMMIT_HASH
    void logExtensionsCommitHashes();
#endif
    std::shared_ptr<EnvConfig>               envConfig_;
    std::shared_ptr<Logger>                  logger_;
    std::shared_ptr<IDeviceManager>          deviceManager_;
    std::shared_ptr<FrameMemoryPool>         frameMemoryPool_;
    std::shared_ptr<StreamIntrinsicsManager> streamIntrinsicsManager_;
    std::shared_ptr<StreamExtrinsicsManager> streamExtrinsicsManager_;
    std::shared_ptr<FilterFactory>           filterFactory_;
    std::shared_ptr<Platform>                platform_;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_context_t {
    std::shared_ptr<libobsensor::Context> context;
};
#ifdef __cplusplus
}
#endif
