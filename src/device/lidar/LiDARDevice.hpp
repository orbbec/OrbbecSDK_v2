// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class LiDARDevice : public DeviceBase {
public:
    LiDARDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~LiDARDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void fetchDeviceInfo() override;
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

private:
};

}  // namespace libobsensor
