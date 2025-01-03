#include "OpenNIVideoSensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"
#include "utils/BufferParser.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {

OpenNIVideoSensor::OpenNIVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {

}

void OpenNIVideoSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    VideoSensor::start(sp, callback);
}

void OpenNIVideoSensor::stop() {
    auto propServer = owner_->getPropertyServer();
    /*BEGIN_TRY_EXECUTE({
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Failed to send the {} stream stop command!", utils::obSensorToStr(sensorType_)); })*/
    VideoSensor::stop();
}

}  // namespace libobsensor
