#include "G2DeviceInfo.hpp"
#include "G2Device.hpp"
#include "G2XLDevice.hpp"
#include "usb/UsbPortGroup.hpp"
#include "DevicePids.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> G2DeviceNameMap = { { 0x0670, "Gemini2" }, { 0x0673, "Gemini2 L" }, { 0x0671, "Gemini2 XL" } };

G2DeviceInfo::G2DeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        auto iter = G2DeviceNameMap.find(portInfo->pid);
        if(iter != G2DeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Gemini2 series device";
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

        auto iter = G2DeviceNameMap.find(portInfo->pid);
        if(iter != G2DeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Gemini2 series device";
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

G2DeviceInfo::~G2DeviceInfo() noexcept {}

std::shared_ptr<IDevice> G2DeviceInfo::createDevice() const {
    if(pid_ == 0x0671) {
        if(IS_NET_PORT(sourcePortInfoList_.front()->portType)) {
            return std::make_shared<G2XLNetDevice>(shared_from_this());
        }
        return std::make_shared<G2XLUSBDevice>(shared_from_this());
    }
    return std::make_shared<G2Device>(shared_from_this());
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G2DeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> G2DeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, Gemini2DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 4) {
            auto info = std::make_shared<G2DeviceInfo>(*iter);
            G2DeviceInfos.push_back(info);
        }
        iter++;
    }

    return G2DeviceInfos;
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> G2DeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> gemini2DeviceInfos;
    auto                                          remainder = FilterNetPortInfoByPid(infoList, Gemini2DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8888), portInfo->port, OB_STREAM_COLOR,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_DEPTH,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8555), portInfo->port, OB_STREAM_IR_LEFT,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8556), portInfo->port, OB_STREAM_IR_RIGHT,
                                                                    portInfo->mac, portInfo->serialNumber, portInfo->pid));
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(portInfo->address, static_cast<uint16_t>(8900), portInfo->port, portInfo->mac,
                                                                       portInfo->serialNumber, portInfo->pid));

            auto deviceEnumInfo = std::make_shared<G2DeviceInfo>(*iter);
            gemini2DeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return gemini2DeviceInfos;
}

}  // namespace libobsensor
