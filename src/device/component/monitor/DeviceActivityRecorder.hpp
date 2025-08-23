// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceActivityRecorder.hpp"
#include "ISourcePort.hpp"
#include "DeviceComponentBase.hpp"

#include <array>
#include <atomic>

namespace libobsensor {
class DeviceActivityRecorder : public IDeviceActivityRecorder, public DeviceComponentBase {
public:
    DeviceActivityRecorder(IDevice *owner);
    virtual ~DeviceActivityRecorder() noexcept override;

    void     touch(DeviceActivity activity) override;
    uint64_t getElapsedSinceLastActive(DeviceActivity activity) const override;
    uint64_t getElapsedSinceLastActive() const override;

private:
    /**
     * @brief Get current time in millisecond
     */
    uint64_t getNow() const;

private:
    static constexpr uint32_t                         activityCount_ = static_cast<uint32_t>(DeviceActivity::Count);
    std::array<std::atomic<uint64_t>, activityCount_> lastActive_{};
    std::atomic<uint64_t>                             lastActiveOverall_{ 0 };
};

}  // namespace libobsensor
