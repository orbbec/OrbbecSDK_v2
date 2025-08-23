// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include <vector>
#include <functional>
#include <string>

namespace libobsensor {

enum class DeviceActivity : uint32_t {
    Stream = 0,  // Stream
    Command,     // Command
    Other,       // Other
    Count,       // For count only
};

class IDeviceActivityRecorder {
public:
    virtual ~IDeviceActivityRecorder() = default;

    /**
     * @brief Touch the device activity (refresh last active time)
     *
     * @param activity The activity type to refresh
     */
    virtual void touch(DeviceActivity activity) = 0;

    /**
     * @brief Get the last active timestamp of the specified activity type
     *        (milliseconds since steady_clock epoch).
     *
     * @param activity The activity type
     * @return Timestamp in milliseconds since steady_clock epoch
     */
    virtual uint64_t getLastActive(DeviceActivity activity) const = 0;

    /**
     * @brief Get the last active timestamp among all activity types
     *        (milliseconds since steady_clock epoch).
     *
     * @return Timestamp in milliseconds since steady_clock epoch
     */
    virtual uint64_t getLastActive() const = 0;
};

}  // namespace libobsensor
