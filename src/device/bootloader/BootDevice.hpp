// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class BootDevice : public DeviceBase {
public:
    BootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~BootDevice() noexcept;

private:
    void init() override;
};

}  // namespace libobsensor
