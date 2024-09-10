
#include "G330NetDisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "component/property/InternalProperty.hpp"
#include "utils/BufferParser.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {
G330NetDisparitySensor::G330NetDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : DisparityBasedSensor(owner, sensorType, backend) {
}

void G330NetDisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto currentVSP = sp->as<VideoStreamProfile>();

    OBInternalVideoStreamProfile vsp = { 0 };
    vsp.sensorType                   = (uint16_t)utils::mapStreamTypeToSensorType(sp->getType());
    vsp.formatFourcc                 = utils::obFormatToUvcFourcc(sp->getFormat());
    vsp.width                        = currentVSP->getWidth();
    vsp.height                       = currentVSP->getHeight();
    vsp.fps                          = currentVSP->getFps();

    auto propServer = owner_->getPropertyServer();
    BEGIN_TRY_EXECUTE({
        propServer->setStructureDataT<OBInternalVideoStreamProfile>(OB_STRUCT_DEPTH_STREAM_PROFILE, vsp);
        DisparityBasedSensor::start(sp, callback);
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, true);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Start {} stream failed!", utils::obSensorToStr(sensorType_));
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, false);
    })
}
    
void G330NetDisparitySensor::stop() {
    auto propServer = owner_->getPropertyServer();
    BEGIN_TRY_EXECUTE({
        DisparityBasedSensor::stop();
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, false);
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Start {} stream failed!", utils::obSensorToStr(sensorType_)); })
}

void G330NetDisparitySensor::updateStreamProfileList(const StreamProfileList &profileList) {
    DisparityBasedSensor::updateStreamProfileList(profileList);
}

}  // namespace libobsensor