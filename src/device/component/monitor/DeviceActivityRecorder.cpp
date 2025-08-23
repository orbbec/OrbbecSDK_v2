// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceActivityRecorder.hpp"

namespace libobsensor {

DeviceActivityRecorder::DeviceActivityRecorder(IDevice *owner) : DeviceComponentBase(owner) {}

DeviceActivityRecorder::~DeviceActivityRecorder() noexcept {}

void DeviceActivityRecorder::touch(DeviceActivity activity) {
    auto index = static_cast<uint32_t>(activity);
    if(index < DeviceActivityRecorder::activityCount_) {
        auto now = getNow();
        lastActive_[index].store(now, std::memory_order_relaxed);
        lastActiveOverall_.store(now, std::memory_order_relaxed);
    }
}

uint64_t DeviceActivityRecorder::getLastActive(DeviceActivity activity) const {
    auto index = static_cast<uint32_t>(activity);
    if(index < DeviceActivityRecorder::activityCount_) {
        return lastActive_[index].load(std::memory_order_relaxed);
    }
    return 0;
}

uint64_t DeviceActivityRecorder::getLastActive() const {
    return lastActiveOverall_.load(std::memory_order_relaxed);
}

}  // namespace libobsensor
