// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BootDeviceInfo.hpp"
#include "BootDevice.hpp"
#include "usb/UsbPortGroup.hpp"
#include "DevicePids.hpp"

#include <map>

namespace libobsensor {

BootDeviceInfo::BootDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

    name_ = "Bootloader device";

    fullName_ = "Orbbec " + name_;

    pid_                = portInfo->pid;
    vid_                = portInfo->vid;
    uid_                = portInfo->uid;
    deviceSn_           = portInfo->serial;
    connectionType_     = portInfo->connSpec;
    sourcePortInfoList_ = groupedInfoList;
}

BootDeviceInfo::~BootDeviceInfo() noexcept {}

std::shared_ptr<IDevice> BootDeviceInfo::createDevice() const {
    return std::make_shared<BootDevice>(shared_from_this());
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, BootDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto info = std::make_shared<BootDeviceInfo>(*iter);
            BootDeviceInfos.push_back(info);
        }
        iter++;
    }

    return BootDeviceInfos;
}

}  // namespace libobsensor
