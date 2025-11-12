// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305DeviceInfo.hpp"
#include "G305Device.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> G305DeviceNameMap = {
    { 0x0840, "Gemini 305" },
    { 0x0841, "Gemini 305" },
};

G305DeviceInfo::G305DeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());
        auto iter     = G305DeviceNameMap.find(portInfo->pid);
        if(iter != G305DeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Gemini 305 series device";
        }

        fullName_ = "Orbbec " + name_;

        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else {
        throw invalid_value_exception("Invalid port type");
    }
}

G305DeviceInfo::~G305DeviceInfo() noexcept {}

std::shared_ptr<IDevice> G305DeviceInfo::createDevice() const {
    return std::make_shared<G305Device>(shared_from_this());
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G305DeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G305DeviceInfos;

    // pick usb device
    auto remainder = FilterUSBPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, G305DevPids);
    auto groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 2) {
            auto info = std::make_shared<G305DeviceInfo>(*iter);
            G305DeviceInfos.push_back(info);
        }
        iter++;
    }
    return G305DeviceInfos;
}

}  // namespace libobsensor
