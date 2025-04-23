// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

constexpr uint32_t G335LE_10M_NET_BAND_WIDTH   = 10;
constexpr uint32_t G335LE_1000M_NET_BAND_WIDTH = 1000;

class G330DeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<G330DeviceInfo> {
public:
    G330DeviceInfo(const SourcePortInfoList groupedInfoList);
    ~G330DeviceInfo() noexcept override;

    std::shared_ptr<IDevice> createDevice() const override;

    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList infoList);
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickNetDevices(const SourcePortInfoList infoList);
};

}  // namespace libobsensor

