// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"

namespace libobsensor {

/**
 * @brief LiDARDeviceBase class for LiDAR device
 */
class LiDARDeviceBase : public DeviceBase {
public:
    LiDARDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~LiDARDeviceBase() noexcept override;

    int getFirmwareVersionInt() override;

protected:
    std::string Uint8toString(const std::vector<uint8_t> &data, const std::string &defValue);
};

}  // namespace libobsensor
