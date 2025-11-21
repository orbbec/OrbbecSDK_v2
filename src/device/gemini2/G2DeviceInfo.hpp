// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

class G2DeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<G2DeviceInfo> {
public:
    G2DeviceInfo(const SourcePortInfoList groupedInfoList);
    ~G2DeviceInfo() noexcept override;

    std::shared_ptr<IDevice> createDevice(OBDeviceAccessMode accessMode) const override;
#if defined(BUILD_USB_PAL)
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList infoList);
#endif
#if defined(BUILD_NET_PAL)
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickNetDevices(const SourcePortInfoList infoList);
#endif
};

}  // namespace libobsensor

