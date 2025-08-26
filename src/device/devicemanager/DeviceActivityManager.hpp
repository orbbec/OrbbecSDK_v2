// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "IDeviceActivityRecorder.hpp"

#include <chrono>
#include <string>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace libobsensor {

class DeviceActivityManager {
public:
    DeviceActivityManager();
    virtual ~DeviceActivityManager() = default;

    /**
     * @brief Update activity for the special device
     *
     * @param deviceId The device id
     * @param activityRecorder The device activity recorder
     */
    void update(const std::string &deviceId, std::shared_ptr<IDeviceActivityRecorder> activityRecorder);

    /**
     * @brief Remove all activity for the special device
     *
     * @param deviceId The device id
     */
    void removeActivity(const std::string &deviceId);

    /**
     * @brief Get elapsed milliseconds since last activity of the special device
     *
     * @param activity The activity type
     * @return Elapsed time in milliseconds. -1 means no any activity
     */
    uint64_t getElapsedSinceLastActive(const std::string &deviceId) const;

private:
    /**
     * @brief Get current time in milliseconds since steady_clock epoch
     */
    uint64_t getNow() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

private:
    mutable std::mutex                        mutex_;
    std::unordered_map<std::string, uint64_t> deviceLastActive_;
};

}  // namespace libobsensor
