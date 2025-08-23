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
     * @brief Get elapsed milliseconds since last activity of the given type
     *
     * @param activity The activity type
     * @return Elapsed time in milliseconds
     */
    virtual uint64_t getElapsedSinceLastActive(DeviceActivity activity) const = 0;

    /**
     * @brief Get elapsed milliseconds since the latest activity across all types
     *
     * @return Elapsed time in milliseconds
     */
    virtual uint64_t getElapsedSinceLastActive() const = 0;
};

}  // namespace libobsensor
