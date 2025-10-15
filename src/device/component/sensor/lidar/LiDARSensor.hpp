// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "LiDARStreamer.hpp"

namespace libobsensor {
class LiDARSensor : public SensorBase {
public:
    LiDARSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<LiDARStreamer> &streamer);
    ~LiDARSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:

private:
    std::shared_ptr<LiDARStreamer> streamer_;
};
}  // namespace libobsensor
