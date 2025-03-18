// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "ILiDARStreamer.hpp"

namespace libobsensor {
class LiDARSensor : public SensorBase {
public:
    LiDARSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<ILiDARStreamer> &streamer);
    ~LiDARSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:

private:
    std::shared_ptr<ILiDARStreamer> streamer_;
};
}  // namespace libobsensor
