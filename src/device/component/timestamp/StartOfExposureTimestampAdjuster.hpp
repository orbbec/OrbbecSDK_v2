// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"

#include <atomic>

namespace libobsensor {
class StartOfExposureTimestampAdjuster : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    explicit StartOfExposureTimestampAdjuster(IDevice *device);

    virtual ~StartOfExposureTimestampAdjuster() noexcept override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override {}

private:
    std::atomic<int32_t> syncReferenceMode_{ OB_MIDDLE_OF_EXPOSURE };
};

}  // namespace libobsensor
