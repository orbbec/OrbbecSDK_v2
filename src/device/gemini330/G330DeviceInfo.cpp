// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330DeviceInfo.hpp"
#include "G330Device.hpp"
#include "DabaiADevice.hpp"
#include "DevicePids.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "ethernet/RTPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"
#if defined(BUILD_USB_PAL)
#include "usb/UsbPortGroup.hpp"
#endif
#if defined(BUILD_NET_PAL)
#include "ethernet/NetPortGroup.hpp"
#endif

#include <map>

namespace libobsensor {

G330DeviceInfo::G330DeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        auto vid  = portInfo->vid;
        auto pid  = portInfo->pid;
        auto iter = std::find_if(G330DeviceInfoList.begin(), G330DeviceInfoList.end(),
                                 [vid, pid](const DeviceInfoEntry &entry) { return entry.vid_ == vid && entry.pid_ == pid; });
        if(iter != G330DeviceInfoList.end()) {
            name_     = iter->deviceName_;
            fullName_ = std::string(iter->manufacturer_) + " " + name_;
        }
        else {
            name_     = "Unknown series device";
            fullName_ = name_;
        }

        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        auto vid  = portInfo->vid;
        auto pid  = portInfo->pid;
        auto iter = std::find_if(G330DeviceInfoList.begin(), G330DeviceInfoList.end(),
                                 [vid, pid](const DeviceInfoEntry &entry) { return entry.vid_ == vid && entry.pid_ == pid; });
        if(iter != G330DeviceInfoList.end()) {
            name_     = iter->deviceName_;
            fullName_ = std::string(iter->manufacturer_) + " " + name_;
        }
        else {
            name_     = "Unknown series device";
            fullName_ = name_;
        }

        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
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

std::shared_ptr<IDevice> G330DeviceInfo::createDevice(OBDeviceAccessMode accessMode) const {
    if(connectionType_ == "Ethernet") {
#if defined(BUILD_NET_PAL)
        return std::make_shared<G330NetDevice>(shared_from_this(), accessMode);
#else
        LOG_ERROR("Ethernet pal not supported, please rebuild with BUILD_NET_PAL=ON");
        return nullptr;
#endif
    }
    else {
        if(isDeviceInContainer(DaBaiADevPids, vid_, pid_)) {
            return std::make_shared<DabaiADevice>(shared_from_this());
        }
        return std::make_shared<G330Device>(shared_from_this());
    }
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;

    // pick usb device
    auto remainder = FilterUSBPortInfoByVidPid(infoList, G330DevPids);
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
#endif

#if defined(BUILD_NET_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, G330DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());
            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20000), portInfo->port, OB_STREAM_COLOR, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->vid, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20100), portInfo->port, OB_STREAM_DEPTH, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->vid, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20200), portInfo->port, OB_STREAM_IR_LEFT, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->vid, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20300), portInfo->port, OB_STREAM_IR_RIGHT, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->vid, portInfo->pid));

            iter->emplace_back(std::make_shared<RTPStreamPortInfo>(portInfo->netInterfaceName, portInfo->localMac, portInfo->localAddress, portInfo->address,
                                                                   static_cast<uint16_t>(20400), portInfo->port, OB_STREAM_ACCEL, portInfo->mac,
                                                                   portInfo->serialNumber, portInfo->vid, portInfo->pid));

            auto deviceEnumInfo = std::make_shared<G330DeviceInfo>(*iter);
            G330DeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return G330DeviceInfos;
}
#endif

}  // namespace libobsensor
