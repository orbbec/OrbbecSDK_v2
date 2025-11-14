// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ISourcePort.hpp"
#include "common/DeviceSeriesInfo.hpp"

#include <functional>
#include <algorithm>
namespace libobsensor {

bool GroupUSBSourcePortBySN(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);
bool GroupUSBSourcePortByUrl(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);

// Filter function for std::vector<std::pair<uint16_t, uint16_t>> (matching both vid and pid)
template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterUSBPortInfoByVidPid(const std::vector<T>                &devInfos,
                                                                             const std::vector<DeviceIdentifier> &vidPidEntries) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_USB_PORT(item->portType)) {
            auto dev = std::dynamic_pointer_cast<const USBSourcePortInfo>(item);
            auto it  = std::find_if(vidPidEntries.begin(), vidPidEntries.end(),
                                    [&](const DeviceIdentifier &entry) { return entry.vid_ == dev->vid && entry.pid_ == dev->pid; });
            if(it == vidPidEntries.end()) {
                continue;
            }
            outDeviceInfos.push_back(item);
        }
    }
    return outDeviceInfos;
}

// Filter function for std::vector<uint16_t> (matching only pid)
template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterUSBPortInfoByVidPid(const std::vector<T> &devInfos, uint16_t vid,
                                                                             const std::vector<uint16_t> &pidEntries) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_USB_PORT(item->portType)) {
            auto dev = std::dynamic_pointer_cast<const USBSourcePortInfo>(item);
            if(dev->vid != vid) {
                continue;
            }
            auto it = std::find(pidEntries.begin(), pidEntries.end(), dev->pid);
            if(it == pidEntries.end()) {
                continue;
            }
            outDeviceInfos.push_back(item);
        }
    }
    return outDeviceInfos;
}

}  // namespace libobsensor
