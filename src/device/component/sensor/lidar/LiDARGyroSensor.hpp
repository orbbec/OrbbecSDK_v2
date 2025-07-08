// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "IStreamer.hpp"

namespace libobsensor {
class LiDARGyroSensor : public SensorBase {
public:
    LiDARGyroSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer);
    ~LiDARGyroSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:
    std::shared_ptr<IStreamer> streamer_;
};
}  // namespace libobsensor
