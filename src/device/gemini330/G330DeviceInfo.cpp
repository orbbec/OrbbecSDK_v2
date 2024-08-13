#include "G330DeviceInfo.hpp"
#include "G330Device.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "utils/Utils.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> G300DeviceNameMap = {
    { 0x06d0, "Gemini 2R" },     { 0x06d1, "Gemini 2RL" },   { 0x0800, "Gemini 335" },   { 0x0801, "Gemini 330" },
    { 0x0802, "Gemini dm330" },  { 0x0803, "Gemini 336" },   { 0x0804, "Gemini 335L" },  { 0x0805, "Gemini 330L" },
    { 0x0806, "Gemini dm330L" }, { 0x0807, "Gemini 336L" },  { 0x080B, "Gemini 335Lg" }, { 0x080C, "Gemini 330Lg" },
    { 0x080D, "Gemini 336Lg" },  { 0x080E, "Gemini 335Le" }, { 0x080F, "Gemini 330Le" }, { 0x0810, "Gemini 336Le" },
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
        return std::make_shared<G330Device>(shared_from_this());
    }
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G330DeviceInfos;

    // pick usb device
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, G330DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 3) {
            auto info = std::make_shared<G330DeviceInfo>(*iter);
            G330DeviceInfos.push_back(info);
        }
        iter++;
    }

    // pick ethernet device
    remainder = FilterNetPortInfoByPid(infoList, G330DevPids);
    groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 3) {
            auto info = std::make_shared<G330DeviceInfo>(*iter);
            G330DeviceInfos.push_back(info);
        }
        iter++;
    }

    return G330DeviceInfos;
}

}  // namespace libobsensor
