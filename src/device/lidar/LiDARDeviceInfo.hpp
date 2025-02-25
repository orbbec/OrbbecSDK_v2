
// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {
class LiDARDeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<LiDARDeviceInfo> {
public:
    LiDARDeviceInfo(const SourcePortInfoList groupedInfoList);
    ~LiDARDeviceInfo() noexcept;

    std::shared_ptr<IDevice>                             createDevice() const override;
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickNetDevices(const SourcePortInfoList infoList);
};

}  // namespace libobsensor
