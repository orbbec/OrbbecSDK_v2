#pragma once
#include "sensor/video/VideoSensor.hpp"
#include "component/property/InternalProperty.hpp"

namespace libobsensor {

class OpenNIVideoSensor : public VideoSensor
{
public:
    OpenNIVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~OpenNIVideoSensor() override = default;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback);
    void stop();

private:

};

}