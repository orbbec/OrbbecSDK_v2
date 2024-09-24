#include "Astra2DeviceInfo.hpp"
#include "Astra2Device.hpp"
#include "usb/UsbPortGroup.hpp"
#include "DevicePids.hpp"

#include <map>

namespace libobsensor {

const std::map<int, std::string> Astra2DeviceNameMap = { { 0x0660, "Astra2" } };

Astra2DeviceInfo::Astra2DeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

    auto iter = Astra2DeviceNameMap.find(portInfo->pid);
    if(iter != Astra2DeviceNameMap.end()) {
        name_ = iter->second;
    }
    else {
        name_ = "Astra2 series device";
    }

    fullName_ = "Orbbec " + name_;

    pid_                = portInfo->pid;
    vid_                = portInfo->vid;
    uid_                = portInfo->uid;
    deviceSn_           = portInfo->serial;
    connectionType_     = portInfo->connSpec;
    sourcePortInfoList_ = groupedInfoList;
}

Astra2DeviceInfo::~Astra2DeviceInfo() noexcept {}

std::shared_ptr<IDevice> Astra2DeviceInfo::createDevice() const {
    return std::make_shared<Astra2Device>(shared_from_this());
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> Astra2DeviceInfo::createDeviceInfos(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> Astra2DeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, Astra2DevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 4) {
            auto info = std::make_shared<Astra2DeviceInfo>(*iter);
            Astra2DeviceInfos.push_back(info);
        }
        iter++;
    }

    return Astra2DeviceInfos;
}

}  // namespace libobsensor
