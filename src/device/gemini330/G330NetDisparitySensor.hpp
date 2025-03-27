
#pragma once

#include "sensor/video/DisparityBasedSensor.hpp"

namespace libobsensor {
class G330NetDisparitySensor : public DisparityBasedSensor {
public:
    G330NetDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend, uint32_t linkSpeed);
    virtual ~G330NetDisparitySensor() noexcept;
    
    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

    void setStreamProfileList(const StreamProfileList &profileList) override;

private:
    uint32_t linkSpeed_;

};
}  // namespace libobsensor