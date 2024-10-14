// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceClockSynchronizer.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {
class DeviceClockSynchronizer : public IDeviceClockSynchronizer, public DeviceComponentBase {
public:
    DeviceClockSynchronizer(IDevice *owner, uint64_t deviceClockFreqIn = 1000, uint64_t deviceClockFreqOut = 1000);
    virtual ~DeviceClockSynchronizer() = default;

    void                         setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) override;
    OBDeviceTimestampResetConfig getTimestampResetConfig() override;
    void                         timestampReset() override;
    void                         timerSyncWithHost() override;

private:
    uint64_t deviceClockFreqIn_;
    uint64_t deviceClockFreqOut_;

    std::atomic<bool>            isTimestampResetConfigInit_;
    OBDeviceTimestampResetConfig currentTimestampResetConfig_;
};

}  // namespace libobsensor

