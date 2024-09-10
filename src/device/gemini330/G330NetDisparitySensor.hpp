
#pragma once

#include "sensor/video/DisparityBasedSensor.hpp"

namespace libobsensor {
class G330NetDisparitySensor : public DisparityBasedSensor {
public:
    G330NetDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~G330NetDisparitySensor() override = default;
    
    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

    void updateStreamProfileList(const StreamProfileList &profileList) override;

};
}  // namespace libobsensor