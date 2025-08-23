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

uint64_t DeviceActivityRecorder::getElapsedSinceLastActive(DeviceActivity activity) const {
    auto index = static_cast<uint32_t>(activity);
    if(index < DeviceActivityRecorder::activityCount_) {
        auto now = getNow();
        auto last = lastActive_[index].load(std::memory_order_relaxed);
        return now - last;
    }
    return 0;
}

uint64_t DeviceActivityRecorder::getElapsedSinceLastActive() const {
    auto now  = getNow();
    auto last = lastActiveOverall_.load(std::memory_order_relaxed);
    return now - last;
}

uint64_t DeviceActivityRecorder::getNow() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace libobsensor
