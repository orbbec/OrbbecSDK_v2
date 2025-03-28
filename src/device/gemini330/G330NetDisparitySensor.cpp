
#include "G330NetDisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "component/property/InternalProperty.hpp"
#include "utils/BufferParser.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"
#include "ethernet/RTPStreamPort.hpp"
#include "G330DeviceInfo.hpp"

namespace libobsensor {
G330NetDisparitySensor::G330NetDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend, uint32_t linkSpeed)
    : DisparityBasedSensor(owner, sensorType, backend), linkSpeed_(linkSpeed) {}

void G330NetDisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    if(linkSpeed_ <= G335LE_10M_NET_BAND_WIDTH) {
        throw libobsensor::unsupported_operation_exception(utils::string::to_string()
                                                           << "Failed to start " << sensorType_ << " stream: the ethernet link speed is limited to "
                                                           << linkSpeed_ << "Mb/s! Please check the ethernet connection and reconnect the device.");
    }

    auto currentVSP = sp->as<VideoStreamProfile>();

    auto rtpStreamPort = std::dynamic_pointer_cast<RTPStreamPort>(backend_);
    uint16_t port = rtpStreamPort->getStreamPort();
    LOG_DEBUG("Start {} stream port: {}", utils::obSensorToStr(sensorType_), port);

    OBInternalVideoStreamProfile vsp = { 0 };
    vsp.sensorType                   = (uint16_t)utils::mapStreamTypeToSensorType(sp->getType());
    vsp.formatFourcc                 = utils::obFormatToUvcFourcc(sp->getFormat());
    vsp.width                        = currentVSP->getWidth();
    vsp.height                       = currentVSP->getHeight();
    vsp.fps                          = currentVSP->getFps();
    vsp.port                         = port;

    auto propServer = owner_->getPropertyServer();
    propServer->setStructureDataT<OBInternalVideoStreamProfile>(OB_STRUCT_DEPTH_STREAM_PROFILE, vsp);
    DisparityBasedSensor::start(sp, callback);
    BEGIN_TRY_EXECUTE({
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, true);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Start {} stream failed!", utils::obSensorToStr(sensorType_));
        DisparityBasedSensor::stop();
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, false);
    })
}
    
void G330NetDisparitySensor::stop() {
    auto propServer = owner_->getPropertyServer();
    BEGIN_TRY_EXECUTE({
        propServer->setPropertyValueT<bool>(OB_PROP_START_DEPTH_STREAM_BOOL, false);
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Failed to send the {} stream stop command!", utils::obSensorToStr(sensorType_)); })
    DisparityBasedSensor::stop();
}

void G330NetDisparitySensor::setStreamProfileList(const StreamProfileList &profileList) {
    DisparityBasedSensor::setStreamProfileList(profileList);
}

G330NetDisparitySensor::~G330NetDisparitySensor() noexcept {
}

}  // namespace libobsensor