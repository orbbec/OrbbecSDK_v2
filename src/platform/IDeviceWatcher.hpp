// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <functional>
#include <string>

namespace libobsensor {

typedef enum {
    OB_DEVICE_REMOVED,
    OB_DEVICE_ARRIVAL,
} OBDeviceChangedType;

/**
 * @brief Callback for device status changes.
 *
 * @param changeType Type of device change (e.g., connected, disconnected).
 * @param devUid Unique identifier of the device.
 * @return true if the event was handled successfully, false otherwise.
 */
typedef std::function<bool(OBDeviceChangedType changeType, std::string devUid)> deviceChangedCallback;

class IDeviceWatcher {
public:
    virtual ~IDeviceWatcher() noexcept = default;

    virtual void start(deviceChangedCallback callback) = 0;
    virtual void stop()                                = 0;
};

}  // namespace libobsensor
