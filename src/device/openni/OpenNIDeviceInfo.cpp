// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIDeviceInfo.hpp"
#include "DevicePids.hpp"
#include "utils/Utils.hpp"
#include "OpenNIDeviceBase.hpp"
#include "DaBaiDevice.hpp"
#include "MaxDevice.hpp"
#include "DW2Device.hpp"
#include "AstraMiniDevice.hpp"
#include "exception/ObException.hpp"
#include "SourcePortInfo.hpp"
#if defined(BUILD_USB_PAL)
#include "usb/UsbPortGroup.hpp"
#endif
#if defined(BUILD_NET_PAL)
#include "ethernet/NetPortGroup.hpp"
#include "ethernet/NetDataStreamPort.hpp"
#endif

#include <map>
namespace libobsensor {

const std::map<int, std::string> OpenNIDeviceNameMap = {
    { 0x060e, "DaBai" },         { 0x0655, "DaBai Pro" },        { 0x0659, "DaBai DCW" },      { 0x065a, "DaBai DW" },   { 0x065c, "Gemini E" },
    { 0x065d, "Gemini E Lite" }, { 0x065e, "Astra Mini S Pro" }, { 0x065b, "Astra Mini Pro" }, { 0x069a, "DaBai Max" },  { 0x069e, "DaBai Max Pro" },
    { 0x06aa, "Gemini UW" },     { 0x069f, "DaBai DW2" },        { 0x06a7, "Gemini EW Lite" }, { 0x06a0, "DaBai DCW2" }, { 0x06a6, "Gemini EW" },
    { 0x069d, "Astra Pro2" },    { 0x0657, "DaBai DC1" },
};

const std::map<int, int> depthRgbPidMaps = {
    { 0x060e, 0x050e },  // Dabai
    { 0x0657, 0x0557 },  // Dabai DC1
    { 0x0659, 0x0559 },  // Dabai DCW
    { 0x065c, 0x055c },  // Gemini E
    { 0x06a0, 0x0561 },  // Dabai DCW2
    { 0x06a6, 0x05a6 },  // Gemini EW
    { 0x069e, 0x0560 },  // Dabai Max Pro
    { 0x06aa, 0x05aa }   // Gemini UW
};

OpenNIDeviceInfo::OpenNIDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());
    auto iter     = OpenNIDeviceNameMap.find(portInfo->pid);
    if(iter != OpenNIDeviceNameMap.end()) {
        name_ = iter->second;
    }
    else {
        name_ = "OpenNI series device";
    }

    fullName_           = "Orbbec " + name_;
    pid_                = portInfo->pid;
    vid_                = portInfo->vid;
    uid_                = portInfo->uid;
    deviceSn_           = portInfo->serial;
    connectionType_     = portInfo->connSpec;
    sourcePortInfoList_ = groupedInfoList;
}

OpenNIDeviceInfo::~OpenNIDeviceInfo() noexcept {}

std::shared_ptr<IDevice> OpenNIDeviceInfo::createDevice() const {
    if(pid_ == OB_DEVICE_DABAI_DC1_PID) {
        return std::make_shared<DaBaiDevice>(shared_from_this());
    }

    if(pid_ == OB_DEVICE_MAX_PRO_PID || pid_ == OB_DEVICE_GEMINI_UW_PID || pid_ == OB_DEVICE_DABAI_MAX_PID) {
        return std::make_shared<MaxDevice>(shared_from_this());
    }

    if(pid_ == OB_DEVICE_MINI_PRO_PID || pid_ == OB_DEVICE_MINI_S_PRO_PID || pid_ == OB_DEVICE_ASTRA_PRO2_PID) {
        return std::make_shared<AstraMiniDevice>(shared_from_this());
    }

    if(pid_ == OB_DEVICE_DABAI_DW2_PID || pid_ == OB_DEVICE_DABAI_DCW2_PID || pid_ == OB_DEVICE_GEMINI_EW_PID || pid_ == OB_DEVICE_GEMINI_EW_LITE_PID) {
        return std::make_shared<DW2Device>(shared_from_this());
    }

    return std::make_shared<OpenNIDeviceBase>(shared_from_this());
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> OpenNIDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> OpenNIDeviceInfos;
    auto                                          uvcRemainder   = FilterUSBPortInfoByPid(infoList, OpenniRgbPids);
    auto                                          depthRemainder = FilterUSBPortInfoByPid(infoList, OpenNIDevPids);

    auto groups = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(depthRemainder, GroupUSBSourcePortByUrl);
    auto iter   = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 2) {
            auto item = iter->begin();
            auto depthPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(*item);
            if(utils::isMatchDeviceByPid(depthPortInfo->pid, OpenniAstraPids)) {
                auto info = std::make_shared<OpenNIDeviceInfo>(*iter);
                OpenNIDeviceInfos.push_back(info);
            }
            else {
                auto mapItem = depthRgbPidMaps.find((int)depthPortInfo->pid);
                if(mapItem == depthRgbPidMaps.end()) {
                    continue;
                }

               auto rgbDevicePid = mapItem->second;
               for(auto uvcInfoIter = uvcRemainder.begin(); uvcInfoIter != uvcRemainder.end();) {
                   auto uvcPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(*uvcInfoIter);
                   if((rgbDevicePid == uvcPortInfo->pid) && (depthPortInfo->hubId == uvcPortInfo->hubId)) {
                       iter->push_back(*uvcInfoIter);
                       uvcInfoIter = uvcRemainder.erase(uvcInfoIter);
                       break;
                   }
                   uvcInfoIter++;
               }
               auto info = std::make_shared<OpenNIDeviceInfo>(*iter);
               OpenNIDeviceInfos.push_back(info);
            }
        }

        iter++;
    }
    return OpenNIDeviceInfos;
}
#endif

}  // namespace libobsensor
