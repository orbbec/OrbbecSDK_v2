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
     * @brief Notify that the specified device has started a reboot action.
     * @param deviceId The device id (reboot not necessarily finished).
     */
    void notifyDeviceReboot(const std::string &deviceId);

    /**
     * @brief Remove all activity for the special device
     *
     * @param deviceId The device id
     */
    void removeActivity(const std::string &deviceId);

    /**
     * @brief Get elapsed milliseconds since last activity of the special device
     *
     * @param deviceId The device id
     * @return Elapsed time in milliseconds. -1 means no any activity
     */
    int64_t getElapsedSinceLastActive(const std::string &deviceId) const;

    /**
     * @brief Check whether the specified device has rebooted since the last status check.
     *
     * @param deviceId The device id
     * @return true/false.
     */
    bool hasDeviceRebooted(const std::string &deviceId) const;

private:
    /**
     * @brief Get current time in milliseconds since steady_clock epoch
     */
    int64_t getNow() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

private:
    mutable std::mutex                        mutex_;
    std::unordered_map<std::string, int64_t>  deviceLastActive_;
    std::unordered_map<std::string, int64_t>  deviceLastReboot_;
};

}  // namespace libobsensor
