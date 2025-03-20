// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BootDeviceInfo.hpp"
#include "BootDevice.hpp"
#include "usb/UsbPortGroup.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "DevicePids.hpp"

#include <map>

namespace libobsensor {

BootDeviceInfo::BootDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
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
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());
        name_         = "Bootloader device";

        fullName_ = "Orbbec " + name_;

        pid_                = portInfo->pid;
        vid_                = 0x2BC5;
        uid_                = portInfo->mac;
        deviceSn_           = portInfo->serialNumber;
        connectionType_     = "Ethernet";
        sourcePortInfoList_ = groupedInfoList;
    }
    else {
        throw invalid_value_exception("Invalid port type");
    }

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

std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;
    auto                                          remainder = FilterNetPortInfoByPid(infoList, BootDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto deviceEnumInfo = std::make_shared<BootDeviceInfo>(*iter);
            G330DeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return G330DeviceInfos;
}

}  // namespace libobsensor
