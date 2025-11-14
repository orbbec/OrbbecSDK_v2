// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BootDeviceInfo.hpp"
#include "BootDevice.hpp"
#include "DevicePids.hpp"
#if defined(BUILD_USB_PAL)
#include "usb/UsbPortGroup.hpp"
#endif
#if defined(BUILD_NET_PAL)
#include "ethernet/NetPortGroup.hpp"
#endif

#include <map>

namespace libobsensor {

BootDeviceInfo::BootDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
        name_               = "Bootloader device";
        fullName_           = name_;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->mac;
        deviceSn_           = portInfo->serialNumber;
        connectionType_     = "Ethernet";
        sourcePortInfoList_ = groupedInfoList;
        name_               = "Bootloader device";
        fullName_           = name_;
    }
    else {
        throw invalid_value_exception("Invalid port type");
    }
}

BootDeviceInfo::~BootDeviceInfo() noexcept {}

std::shared_ptr<IDevice> BootDeviceInfo::createDevice() const {
    return std::make_shared<BootDevice>(shared_from_this());
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, BootDevPids);
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
#endif

#if defined(BUILD_NET_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> BootDeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, BootDevPids);
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
#endif

}  // namespace libobsensor
