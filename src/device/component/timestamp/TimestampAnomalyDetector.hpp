// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include <mutex>

namespace libobsensor {
class TimestampAnomalyDetector : public IFrameTimestampCalculator {
public:
    TimestampAnomalyDetector(IDevice *device);
    virtual ~TimestampAnomalyDetector() noexcept override = default;

    void setCurrentFps(uint32_t fps);
    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

    static constexpr uint32_t kMinTimestampDiffLimit = 5000000;  // 5s

private:
    std::shared_ptr<IDeviceSyncConfigurator> deviceSyncConfigurator_;
    OBMultiDeviceSyncMode                    currentDeviceSyncMode_;
    uint64_t                                 lastTimestamp_;
    uint64_t                                 lastValidTimestamp_;
    uint32_t                                 maxValidTimestampDiff_;
    std::mutex                               mutex_;
};
} // namespace libobsensor