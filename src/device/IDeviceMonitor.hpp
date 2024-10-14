// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include <vector>
#include <functional>
#include <string>

namespace libobsensor {

typedef std::function<void(uint64_t stateCode, const std::string &message)> DeviceStateChangedCallback;

class IDeviceMonitor {
public:
    virtual ~IDeviceMonitor() = default;

    virtual OBDeviceState getCurrentDeviceState() const                                     = 0;
    virtual int           registerStateChangedCallback(DeviceStateChangedCallback callback) = 0;
    virtual void          unregisterStateChangedCallback(int callbackId)                    = 0;

    virtual void enableHeartbeat()  = 0;
    virtual void disableHeartbeat() = 0;
    virtual bool isHeartbeatEnabled() const = 0;
    virtual void pauseHeartbeat()   = 0;
    virtual void resumeHeartbeat()  = 0;

    // for debug and vendor specific purpose
    virtual void sendAndReceiveData(const uint8_t *sendData, uint32_t sendDataSize, uint8_t *receiveData, uint32_t *receiveDataSize) = 0;
};
}  // namespace libobsensor
