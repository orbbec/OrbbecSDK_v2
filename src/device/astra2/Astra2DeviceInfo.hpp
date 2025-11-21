// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

class Astra2DeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<Astra2DeviceInfo> {
public:
    Astra2DeviceInfo(const SourcePortInfoList groupedInfoList);
    ~Astra2DeviceInfo() noexcept override;

    std::shared_ptr<IDevice> createDevice(OBDeviceAccessMode accessMode) const override;
#if defined(BUILD_USB_PAL)
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList infoList);
#endif
};

}  // namespace libobsensor

