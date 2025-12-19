// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

class G305DeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<G305DeviceInfo> {
public:
    G305DeviceInfo(const SourcePortInfoList groupedInfoList);
    ~G305DeviceInfo() noexcept override;

    std::shared_ptr<IDevice> createDevice(OBDeviceAccessMode accessMode) const override;

    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList infoList);
};

}  // namespace libobsensor
