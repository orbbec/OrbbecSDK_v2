#pragma once
#include "sensor/video/VideoSensor.hpp"
#include "component/property/InternalProperty.hpp"

namespace libobsensor {

class G330NetVideoSensor : public VideoSensor
{
public:
    G330NetVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend, uint32_t linkSpeed);
    virtual ~G330NetVideoSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:
    void initStreamPropertyId();

private:
    OBInternalPropertyID streamSwitchPropertyId_;
    OBInternalPropertyID profilesSwitchPropertyId_;
    uint32_t             linkSpeed_;

};

}