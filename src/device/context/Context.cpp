#include "Context.hpp"
#include "utils/Utils.hpp"
#include "devicemanager/DeviceManager.hpp"

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

    utils::unusedVar(configFilePath);  // todo: use to load config file
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

}  // namespace libobsensor
