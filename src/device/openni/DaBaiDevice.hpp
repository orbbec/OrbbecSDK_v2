// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "OpenNIDeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "property/PropertyServer.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class DaBaiDevice : public OpenNIDeviceBase {
public:
    DaBaiDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~DaBaiDevice() noexcept override;

protected:
    virtual void init() override;
    virtual void initSensorList() override;
    virtual void initProperties() override;
};

}  // namespace libobsensor

