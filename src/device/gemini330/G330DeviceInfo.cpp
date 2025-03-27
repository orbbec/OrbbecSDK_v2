// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330DeviceInfo.hpp"
#include "G330Device.hpp"
#include "DabaiADevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "ethernet/RTPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> G300DeviceNameMap = {
    { 0x06d0, "Gemini 2R" },    { 0x06d1, "Gemini 2RL" },   { 0x0800, "Gemini 335" },   { 0x0801, "Gemini 330" },    { 0x0802, "Gemini dm330" },
    { 0x0803, "Gemini 336" },   { 0x0804, "Gemini 335L" },  { 0x0805, "Gemini 330L" },  { 0x0806, "Gemini dm330L" }, { 0x0807, "Gemini 336L" },
    { 0x080B, "Gemini 335Lg" }, { 0x080C, "Gemini 330Lg" }, { 0x080D, "Gemini 336Lg" }, { 0x080E, "Gemini 335Le" },  { 0x080F, "Gemini 330Le" },
    { 0x0A12, "Dabai A" },      { 0x0A13, "Dabai AL" },     { 0x0812, "Gemini 345" },   { 0x0813, "Gemini 345Lg" },  { 0x0816, "CAM-5330" },
    { 0x0817, "CAM-5530" },
};

G330DeviceInfo::G330DeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());
        auto iter = G300DeviceNameMap.find(portInfo->pid);
        if(iter != G300DeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Gemini 300 series device";
        }

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
        auto iter     = G300DeviceNameMap.find(portInfo->pid);
        if(iter != G300DeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Gemini 300 series device";
        }

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

G330DeviceInfo::~G330DeviceInfo() noexcept {}

std::shared_ptr<IDevice> G330DeviceInfo::createDevice() const {
    if(connectionType_ == "Ethernet") {
        return std::make_shared<G330NetDevice>(shared_from_this());
    }
    else {
        if(std::find(DaBaiADevPids.begin(), DaBaiADevPids.end(), pid_) != DaBaiADevPids.end()) {
            return std::make_shared<DabaiADevice>(shared_from_this());
        }
        return std::make_shared<G330Device>(shared_from_this());
    }
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;

    // pick usb device
    auto remainder = FilterUSBPortInfoByPid(infoList, G330DevPids);
    auto groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 3) {
            auto info = std::make_shared<G330DeviceInfo>(*iter);
            G330DeviceInfos.push_back(info);
        }
        iter++;
    }
    return G330DeviceInfos;
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;
    auto                                          remainder = FilterNetPortInfoByPid(infoList, G330DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());
            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20000), portInfo->port, OB_STREAM_COLOR, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20100), portInfo->port, OB_STREAM_DEPTH, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20200), portInfo->port, OB_STREAM_IR_LEFT, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20300), portInfo->port, OB_STREAM_IR_RIGHT, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20400), portInfo->port, OB_STREAM_ACCEL, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->pid));

            auto deviceEnumInfo = std::make_shared<G330DeviceInfo>(*iter);
            G330DeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return G330DeviceInfos;
}

}  // namespace libobsensor
