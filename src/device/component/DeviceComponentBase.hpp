// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"

#include <memory>

namespace libobsensor {

class DeviceComponentBase : virtual public IDeviceComponent {
public:
    DeviceComponentBase(IDevice *owner) : owner_(owner) {}

    IDevice *getOwner() const override {
        return owner_;
    }

private:
    IDevice *owner_;
};

}  // namespace libobsensor
