// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/ObSensor.hpp"
#include <memory>

namespace libobsensor {

/**
 * @brief Interface for device access control.
 *
 * Provides methods to manage and query the control state of a device,
 * including acquisition, release, and keep-alive functionality.
 */
class IDeviceAccessController : public std::enable_shared_from_this<IDeviceAccessController> {
public:
    virtual ~IDeviceAccessController() = default;

    /**
     * @brief Check if the device supports access control.
     *
     * @return true if access control is supported; false otherwise.
     */
    virtual bool isSupported() const = 0;

    /**
     * @brief Get the current access mode state of the device.
     *
     * @return The current device access mode.
     */
    virtual OBDeviceAccessMode getState() const = 0;

    /**
     * @brief Acquire control over the device with the specified access mode,
     *        and start the keep-alive mechanism to maintain the control session
     *
     * @param[in] accessMode The access mode to acquire (e.g., exclusive, control, monitor).
     *
     * @throws access_denied_exception if the acquisition fails.
     *         unsupported_operation_exception if the device doesn't support CCP
     */
    virtual void acquireControl(OBDeviceAccessMode accessMode) = 0;

    /**
     * @brief Release the previously acquired control of the device.
     */
    virtual void releaseControl() = 0;
};

}  // namespace libobsensor
