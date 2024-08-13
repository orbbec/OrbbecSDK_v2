#pragma once
#include "sensor/video/VideoSensor.hpp"

namespace libobsensor {

class G330NetVideoSensor : public VideoSensor
{
public:
    G330NetVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~G330NetVideoSensor() override = default;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback);
    void stop();

private:
    void initStreamProfileList();

private:
    /* data */

};

}