// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"

#include <memory>
#include <vector>
#include <algorithm>

namespace libobsensor {

bool GroupNetSourcePortByMac(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);

template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterNetPortInfoByPid(const std::vector<T> &devInfos, const std::vector<uint16_t> &pids) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_NET_PORT(item->portType)) {
            auto dev  = std::dynamic_pointer_cast<const NetSourcePortInfo>(item);
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
