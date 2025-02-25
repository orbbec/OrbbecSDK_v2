// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDeviceInfo.hpp"
#include "LiDARDevice.hpp"
#include "DevicePids.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"

namespace libobsensor {
const std::map<int, std::string> LiDARDeviceNameMap = {
    // TODO need to change pid and name
    { 0x1234, "Single-line LiDAR MS600" },
    { 0x5678, "Multi-lines LiDAR TL2401" },
};

LiDARDeviceInfo::LiDARDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        auto iter = LiDARDeviceNameMap.find(portInfo->pid);
        if(iter != LiDARDeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "LiDAr series device";
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

LiDARDeviceInfo::~LiDARDeviceInfo() noexcept {}

std::shared_ptr<IDevice> LiDARDeviceInfo::createDevice() const {
    std::shared_ptr<IDevice> device;
    if(connectionType_ == "Ethernet") {
        // TODO to support single and multi line LiDAR
        return std::make_shared<LiDARDevice>(shared_from_this());
    }
    else {
        return nullptr;
    }
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> LiDARDeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> LiDARDeviceInfos;
    auto                                          remainder = FilterNetPortInfoByPid(infoList, LiDARDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            // TODO add real source port info
            // auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());
            // iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8888), portInfo->port, OB_STREAM_COLOR,
            //                                                         portInfo->mac, portInfo->serialNumber, portInfo->pid));
            // iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_DEPTH,
            //                                                         portInfo->mac, portInfo->serialNumber, portInfo->pid));
            // iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_IR,
            // portInfo->mac,
            //                                                         portInfo->serialNumber, portInfo->pid));
            // iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8900), portInfo->port, portInfo->mac,
            //                                                            portInfo->serialNumber, portInfo->pid));  // imu data stream

            auto deviceEnumInfo = std::make_shared<LiDARDeviceInfo>(*iter);
            LiDARDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return LiDARDeviceInfos;
}

}  // namespace libobsensor
