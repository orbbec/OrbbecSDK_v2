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

class IReader {
public:
    virtual ~IReader() = default;

    virtual std::shared_ptr<DeviceInfo>    getDeviceInfo()                                = 0;
    virtual std::chrono::nanoseconds       getDuration()                                  = 0;
    virtual std::shared_ptr<StreamProfile> getStreamProfile(OBStreamType streamType)      = 0;
    virtual bool                           getIsEndOfFile()                               = 0;
    virtual std::vector<OBSensorType>      getSensorTypeList() const                      = 0;
    virtual std::chrono::nanoseconds       getCurTime()                                   = 0;
    virtual std::vector<uint8_t>           getPropertyData(uint32_t propertyId)           = 0;
    virtual bool                           isPropertySupported(uint32_t propertyId) const = 0;

    virtual std::shared_ptr<Frame> readNextData()                                       = 0;
    virtual void                   seekToTime(const std::chrono::nanoseconds &seekTime) = 0;

    virtual void stop() = 0;
};

}  // namespace libobsensor
