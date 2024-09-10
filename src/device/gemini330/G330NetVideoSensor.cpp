#include "G330NetVideoSensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"
#include "utils/BufferParser.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {

G330NetVideoSensor::G330NetVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend),
      streamSwitchPropertyId_(OB_PROP_START_COLOR_STREAM_BOOL),
      profilesSwitchPropertyId_(OB_STRUCT_COLOR_STREAM_PROFILE) {

    initStreamPropertyId();
}

void G330NetVideoSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto currentVSP = sp->as<VideoStreamProfile>();

    OBInternalVideoStreamProfile vsp = { 0 };
    vsp.sensorType                   = (uint16_t)utils::mapStreamTypeToSensorType(sp->getType());
    vsp.formatFourcc                 = utils::obFormatToUvcFourcc(sp->getFormat());
    vsp.width                        = currentVSP->getWidth();
    vsp.height                       = currentVSP->getHeight();
    vsp.fps                          = currentVSP->getFps();

    auto propServer = owner_->getPropertyServer();
    BEGIN_TRY_EXECUTE({
        propServer->setStructureDataT<OBInternalVideoStreamProfile>(profilesSwitchPropertyId_, vsp);
        VideoSensor::start(sp, callback);
        propServer->setPropertyValueT<bool>(streamSwitchPropertyId_, true);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Start {} stream failed!", utils::obSensorToStr(sensorType_));
        propServer->setPropertyValueT<bool>(streamSwitchPropertyId_, false);
    })
}

void G330NetVideoSensor::stop() {
    auto propServer = owner_->getPropertyServer();
    BEGIN_TRY_EXECUTE({
        VideoSensor::stop();
        propServer->setPropertyValueT<bool>(streamSwitchPropertyId_, false);
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Stop {} stream failed!", utils::obSensorToStr(sensorType_)); })
}

void G330NetVideoSensor::initStreamPropertyId() {
    if(sensorType_ == OB_SENSOR_COLOR) {
        streamSwitchPropertyId_   = OB_PROP_START_COLOR_STREAM_BOOL;
        profilesSwitchPropertyId_ = OB_STRUCT_COLOR_STREAM_PROFILE;
    }

    if(sensorType_ == OB_SENSOR_IR_LEFT) {
        streamSwitchPropertyId_   = OB_PROP_START_IR_STREAM_BOOL;
        profilesSwitchPropertyId_ = OB_STRUCT_IR_STREAM_PROFILE;
    }

    if(sensorType_ == OB_SENSOR_IR_RIGHT) {
        streamSwitchPropertyId_   = OB_PROP_START_IR_RIGHT_STREAM_BOOL;
        profilesSwitchPropertyId_ = OB_STRUCT_IR_RIGHT_STREAM_PROFILE;
    }
}

}  // namespace libobsensor
