
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
    initStreamProfileList();
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

void G330NetDisparitySensor::initStreamProfileList() {
    auto propServer = owner_->getPropertyServer();

    StreamProfileList    backendSpList;
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            OB_RAW_DATA_STREAM_PROFILE_LIST,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get {} profile list params failed!", utils::obSensorToStr(sensorType_));
        data.clear();
    })

    if(!data.empty()) {
        std::vector<OBInternalStreamProfile> outputProfiles;
        uint16_t                             dataSize = static_cast<uint16_t>(data.size());
        outputProfiles                                = parseBuffer<OBInternalStreamProfile>(data.data(), dataSize);
        backendSpList.clear();
        for(auto item: outputProfiles) {
            if(sensorType_ == (OBSensorType)item.sensorType) {
                OBStreamType streamType = utils::mapSensorTypeToStreamType((OBSensorType)item.sensorType);
                OBFormat     format     = utils::uvcFourccToOBFormat(item.profile.video.formatFourcc);
                backendSpList.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, format, item.profile.video.width, item.profile.video.height,
                                                                                       item.profile.video.fps));
            }
        }
    }
    else {
        LOG_WARN("Get {} stream profile list failed!", utils::obSensorToStr(sensorType_));
    }

    updateStreamProfileList(backendSpList);
    convertProfileAsDisparityBasedProfile();
}


}  // namespace libobsensor