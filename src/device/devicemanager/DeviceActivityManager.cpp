// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceActivityManager.hpp"
#include <algorithm>

namespace libobsensor {

DeviceActivityManager::DeviceActivityManager() {}

void DeviceActivityManager::update(const std::string &deviceId, std::shared_ptr<IDeviceActivityRecorder> activityRecorder) {
    if(!activityRecorder) {
        return;
    }

    // Get the last active timestamp of the device from recorder
    auto lastActive = activityRecorder->getLastActive();
    // Update the device's last active time (thread-safe)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        deviceLastActive_[deviceId] = lastActive;
    }
}

void DeviceActivityManager::removeActivity(const std::string &deviceId) {
    // Remove all activity records for the device
    std::lock_guard<std::mutex> lock(mutex_);
    deviceLastActive_.erase(deviceId);
}

uint64_t DeviceActivityManager::getElapsedSinceLastActive(const std::string &deviceId) const {
    uint64_t lastActive = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = deviceLastActive_.find(deviceId);
        if(it == deviceLastActive_.end()) {
            return UINT64_MAX;  // return UINT64_MAX if device not found
        }
        lastActive = it->second;
    }

    uint64_t now = getNow();
    return (now > lastActive) ? (now - lastActive) : 0;
}

}  // namespace libobsensor