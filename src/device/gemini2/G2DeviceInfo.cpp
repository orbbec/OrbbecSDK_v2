#include "G2DeviceInfo.hpp"
#include "G2Device.hpp"
#include "usb/UsbPortGroup.hpp"
#include "DevicePids.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> G2DeviceNameMap = { { 0x0670, "Gemini2" }, { 0x0673, "Gemini2 L" }, { 0x0671, "Gemini2 XL" } };

G2DeviceInfo::G2DeviceInfo(const SourcePortInfoList groupedInfoList) {
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

G2DeviceInfo::~G2DeviceInfo() noexcept {}

std::shared_ptr<IDevice> G2DeviceInfo::createDevice() const {
    auto device = std::make_shared<G2Device>(shared_from_this());
    return device;
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

}  // namespace libobsensor
