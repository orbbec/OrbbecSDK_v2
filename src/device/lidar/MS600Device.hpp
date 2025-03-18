// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

/**
 * @brief MS600Device class for LiDAR MS600/SL450 device
 */
class MS600Device : public DeviceBase {
public:
    MS600Device(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~MS600Device() noexcept;

private:
    void        init() override;
    void        initSensorList();
    void        initProperties();
    void        fetchDeviceInfo() override;
    void        initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    std::string Uint8toString(const std::vector<uint8_t> &data, const std::string &default);
};

}  // namespace libobsensor
