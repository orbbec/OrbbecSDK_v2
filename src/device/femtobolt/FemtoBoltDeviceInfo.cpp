#include "FemtoBoltDeviceInfo.hpp"
#include "FemtoBoltDevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "utils/Utils.hpp"
namespace libobsensor {
FemtoBoltDeviceInfo::FemtoBoltDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

    name_               = "FemtoBolt";
    fullName_           = "Orbbec " + name_;
    pid_                = portInfo->pid;
    vid_                = portInfo->vid;
    uid_                = portInfo->uid;
    deviceSn_           = portInfo->serial;
    connectionType_     = portInfo->connSpec;
    sourcePortInfoList_ = groupedInfoList;
}

FemtoBoltDeviceInfo::~FemtoBoltDeviceInfo() noexcept {}

std::shared_ptr<IDevice> FemtoBoltDeviceInfo::createDevice() const {
    auto device = std::make_shared<FemtoBoltDevice>(shared_from_this());
    return device;
}

std::vector<std::shared_ptr<IDeviceEnumInfo>> FemtoBoltDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> femtoBoltDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByPid(infoList, FemtoBoltDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 2) {
            auto info = std::make_shared<FemtoBoltDeviceInfo>(*iter);
            femtoBoltDeviceInfos.push_back(info);
        }
        iter++;
    }
    return femtoBoltDeviceInfos;
}
}  // namespace libobsensor