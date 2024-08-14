#include "FemtoMegaDeviceInfo.hpp"
#include "FemtoMegaDevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "ethernet/NetPortGroup.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
namespace libobsensor {
FemtoMegaDeviceInfo::FemtoMegaDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        name_               = "Femto Mega";
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        name_               = "Femto Mega";
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
        device = std::make_shared<FemtoMegaNetDevice>(shared_from_this());
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

    // pick ethernet device
    remainder = FilterNetPortInfoByPid(infoList, FemtoMegaDevPids);
    groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto info = std::make_shared<FemtoMegaDeviceInfo>(*iter);
            femtoMegaDeviceInfos.push_back(info);
        }
        iter++;
    }

    return femtoMegaDeviceInfos;
}
}  // namespace libobsensor