// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "frame/Frame.hpp"
#include "IDevice.hpp"
#include "libobsensor/h/ObTypes.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace libobsensor {

class IWriter {
public:
    virtual ~IWriter() = default;

    virtual void writeFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame) = 0;
    virtual void writeDeviceInfo(const std::shared_ptr<const DeviceInfo> &deviceInfo)              = 0;
    virtual void writeProperty(uint32_t propertyID, const uint8_t *data, const uint32_t datasize)  = 0;
    virtual void writeStreamProfiles() = 0;
};

}  // namespace libobsensor