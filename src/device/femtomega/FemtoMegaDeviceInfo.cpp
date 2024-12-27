// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FemtoMegaDeviceInfo.hpp"
#include "FemtoMegaDevice.hpp"
#include "FemtoMegaIDevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"
namespace libobsensor {
const std::map<int, std::string> FemtoMegaDeviceNameMap = {
    { 0x06c0, "Femto Mega i" },
    { 0x0669, "Femto Mega" },
};

FemtoMegaDeviceInfo::FemtoMegaDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        auto iter = FemtoMegaDeviceNameMap.find(portInfo->pid);
        if(iter != FemtoMegaDeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Femto Mega series device";
        }
        fullName_           = "Orbbec " + name_;
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        auto iter = FemtoMegaDeviceNameMap.find(portInfo->pid);
        if(iter != FemtoMegaDeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Femto Mega series device";
        }
        fullName_           = "Orbbec " + name_;
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

FemtoMegaDeviceInfo::~FemtoMegaDeviceInfo() noexcept {}

std::shared_ptr<IDevice> FemtoMegaDeviceInfo::createDevice() const {
    std::shared_ptr<IDevice> device;
    if(connectionType_ == "Ethernet") {
        if(IS_OB_FEMTO_MEGA_I_PID(pid_)) {
            // for Femto Mega i
            device = std::make_shared<FemtoMegaINetDevice>(shared_from_this());
        } else {
            device = std::make_shared<FemtoMegaNetDevice>(shared_from_this());
        }
    }
    else {
        device = std::make_shared<FemtoMegaUsbDevice>(shared_from_this());
    }
    return device;
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> FemtoMegaDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> femtoMegaDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, FemtoMegaDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 2) {
            auto info = std::make_shared<FemtoMegaDeviceInfo>(*iter);
            femtoMegaDeviceInfos.push_back(info);
        }
        iter++;
    }

    return femtoMegaDeviceInfos;
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> FemtoMegaDeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> femtoMegaDeviceInfos;
    auto                                          remainder = FilterNetPortInfoByPid(infoList, FemtoMegaDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {

            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8888), portInfo->port, OB_STREAM_COLOR,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_DEPTH,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_IR, portInfo->mac,
                                                                    portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8900), portInfo->port, portInfo->mac,
                                                                       portInfo->serialNumber, portInfo->pid));  // imu data stream

            auto deviceEnumInfo = std::make_shared<FemtoMegaDeviceInfo>(*iter);
            femtoMegaDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return femtoMegaDeviceInfos;
}

}  // namespace libobsensor
