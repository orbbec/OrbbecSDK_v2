#pragma once
#include "sensor/video/VideoSensor.hpp"
#include "component/property/InternalProperty.hpp"

namespace libobsensor {

class G330NetVideoSensor : public VideoSensor
{
public:
    G330NetVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~G330NetVideoSensor() override = default;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback);
    void stop();

private:
    void initStreamPropertyId();
    void initStreamProfileList();

private:
    OBInternalPropertyID streamSwitchPropertyId_;
    OBInternalPropertyID profilesSwitchPropertyId_;

};

}