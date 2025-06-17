// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"
#include "IDeviceSyncConfigurator.hpp"

namespace libobsensor {
class TimestampAnomalyDetector : public IFrameTimestampCalculator {
public:
    TimestampAnomalyDetector(IDevice *device);
    virtual ~TimestampAnomalyDetector() noexcept override = default;

    void setCurrentFps(uint32_t fps);
    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    std::shared_ptr<IDeviceSyncConfigurator> deviceSyncConfigurator_;
    OBMultiDeviceSyncMode                    currentDeviceSyncMode_;
    uint64_t                                 cacheTimestamp_;
    uint32_t                                 maxValidTimestampDiff_;
    uint32_t                                 cacheFps_;
    std::atomic<bool>                        needDetect_;
};
} // namespace libobsensor