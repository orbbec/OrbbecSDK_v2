// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "component/sensor/imu/AccelSensor.hpp"
#include "component/sensor/imu/ImuStreamer.hpp"

namespace libobsensor {
class G330NetAccelSensor : public AccelSensor {
public:
    G330NetAccelSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<ImuStreamer> &streamer);
    ~G330NetAccelSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;
};
}  // namespace libobsensor
