// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AstraMiniSensorStreamStrategy.hpp"
#include "exception/ObException.hpp"
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>

namespace libobsensor {


AstraMiniSensorStreamStrategy::AstraMiniSensorStreamStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

AstraMiniSensorStreamStrategy::~AstraMiniSensorStreamStrategy() noexcept {}

void AstraMiniSensorStreamStrategy::markStreamActivated(const std::shared_ptr<const StreamProfile> &profile) {
    std::lock_guard<std::mutex> lock(startedStreamListMutex_);
    auto                        streamType = profile->getType();
    auto                        iter       = std::find_if(activatedStreamList_.begin(), activatedStreamList_.end(),
                                                          [streamType](const std::shared_ptr<const StreamProfile> &sp) { return sp->getType() == streamType; });
    if(iter != activatedStreamList_.end()) {
        throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has already been started.");
    }

    activatedStreamList_.push_back(profile);
}

void AstraMiniSensorStreamStrategy::markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile) {
    std::lock_guard<std::mutex> lock(startedStreamListMutex_);
    auto                        streamType = profile->getType();
    auto                        iter       = std::find_if(activatedStreamList_.begin(), activatedStreamList_.end(),
                                                          [streamType](const std::shared_ptr<const StreamProfile> &sp) { return sp->getType() == streamType; });
    if(iter == activatedStreamList_.end()) {
        throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has not been started.");
    }
    activatedStreamList_.erase(iter);
}

void AstraMiniSensorStreamStrategy::validateStream(const std::shared_ptr<const StreamProfile> &profile) {
    validateStream(std::vector<std::shared_ptr<const StreamProfile>>{ profile });
}

void AstraMiniSensorStreamStrategy::validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) {
    {
        std::lock_guard<std::mutex> lock(startedStreamListMutex_);
        for(auto profile: profiles) {
            auto streamType = profile->getType();
            auto iter       = std::find_if(activatedStreamList_.begin(), activatedStreamList_.end(),[streamType](const std::shared_ptr<const StreamProfile> &sp) {
                    return sp->getType() == streamType;
            });

            if(iter != activatedStreamList_.end()) {
                throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has already been started.");
            }
        }

        for(auto profile: profiles) {
            auto streamType = profile->getType();
            auto iter = std::find_if(activatedStreamList_.begin(), activatedStreamList_.end(), [streamType](const std::shared_ptr<const StreamProfile> &sp) {
                if(streamType == OB_STREAM_IR && sp->getType() == OB_STREAM_COLOR) {
                    return true;
                }
                if(streamType == OB_STREAM_COLOR && sp->getType() == OB_STREAM_IR) {
                    return true;
                }
                return false;
            });

            if(iter != activatedStreamList_.end()) {
                throw unsupported_operation_exception(utils::string::to_string() << "IR and Color cannot be started simultaneously.");
            }
        }
    }
}


}  // namespace libobsensor
