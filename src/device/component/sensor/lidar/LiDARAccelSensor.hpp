// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "IStreamer.hpp"

namespace libobsensor {
class LiDARAccelSensor : public SensorBase {
public:
    LiDARAccelSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer);
    ~LiDARAccelSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:
    std::shared_ptr<IStreamer> streamer_;
};
}  // namespace libobsensor
