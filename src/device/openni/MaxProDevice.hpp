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

class MaxProDevice : public OpenNIDeviceBase {
public:
    MaxProDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~MaxProDevice() noexcept;

protected:
    virtual void init() override;
    virtual void initSensorList() override;
    virtual void initProperties() override;
    virtual void initSensorStreamProfile(std::shared_ptr<ISensor> sensor) override;

private:


};

}  // namespace libobsensor

