// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"

#include "common/DeviceSeriesInfo.hpp"
#include <memory>
#include <vector>
#include <algorithm>

namespace libobsensor {

bool GroupNetSourcePortByMac(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1);

// Filter function for std::vector<std::pair<uint16_t, uint16_t>> (matching both vid and pid)
template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterNetPortInfoByVidPid(const std::vector<T> &devInfos, const std::vector<DeviceIdentifier> &entries) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_NET_PORT(item->portType)) {
            auto dev = std::dynamic_pointer_cast<const NetSourcePortInfo>(item);
            auto it =
                std::find_if(entries.begin(), entries.end(), [&](const DeviceIdentifier &entry) { return entry.vid_ == dev->vid && entry.pid_ == dev->pid; });

            if(it == entries.end()) {
                continue;
            }
            outDeviceInfos.push_back(item);
        }
    }
    return outDeviceInfos;
}

// Filter function for std::vector<uint16_t> (matching only pid)
template <typename T>
std::vector<std::shared_ptr<const SourcePortInfo>> FilterNetPortInfoByVidPid(const std::vector<T> &devInfos, uint16_t vid,
                                                                             const std::vector<uint16_t> &entries) {
    std::vector<std::shared_ptr<const SourcePortInfo>> outDeviceInfos;
    for(auto &item: devInfos) {
        if(IS_NET_PORT(item->portType)) {
            auto dev = std::dynamic_pointer_cast<const NetSourcePortInfo>(item);
            if(dev->vid != vid) {
                continue;
            }
            auto it = std::find(entries.begin(), entries.end(), dev->pid);
            if(it == entries.end()) {
                continue;
            }
            outDeviceInfos.push_back(item);
        }
    }
    return outDeviceInfos;
}
}  // namespace libobsensor
