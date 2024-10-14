// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ISourcePort.hpp"
#include <functional>
#include <algorithm>
namespace libobsensor {

bool GroupUSBSourcePortBySN(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);
bool GroupUSBSourcePortByUrl(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);

template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterUSBPortInfoByPid(const std::vector<T> &devInfos, const std::vector<uint16_t> &pids) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_USB_PORT(item->portType)) {
            auto dev  = std::dynamic_pointer_cast<const USBSourcePortInfo>(item);
            auto iter = std::find(pids.begin(), pids.end(), dev->pid);
            if(iter == pids.end()) {
                continue;
            }
            outDeviceInfos.push_back(item);
        }
    }
    return outDeviceInfos;
}

}  // namespace libobsensor
