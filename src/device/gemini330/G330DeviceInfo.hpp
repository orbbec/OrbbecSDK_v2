// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec  Corporation. All Rights Reserved.
#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

class G330DeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<G330DeviceInfo> {
public:
    G330DeviceInfo(const SourcePortInfoList groupedInfoList);
    ~G330DeviceInfo() noexcept;

    std::shared_ptr<IDevice> createDevice() const override;

    static std::vector<std::shared_ptr<IDeviceEnumInfo>> createDeviceInfos(const SourcePortInfoList infoList);
};

}  // namespace libobsensor