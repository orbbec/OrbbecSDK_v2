// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/video/VideoSensor.hpp"

namespace libobsensor {
class RawPhaseBasedSensor : public VideoSensor {
public:
    RawPhaseBasedSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    virtual ~RawPhaseBasedSensor() noexcept;

    void refreshStreamProfiles();

protected:
    void trySendStopStreamVendorCmd() override;
};

}  // namespace libobsensor
