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
    initStreamProfileList();
}

void G330NetVideoSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto currentVSP = sp->as<VideoStreamProfile>();

    OBInternalVideoStreamProfile vsp = { 0 };
    vsp.sensorType                   = (uint16_t)utils::mapStreamTypeToSensorType(sp->getType());
    vsp.formatFourcc                 = utils::obFormatToUvcFourcc(sp->getFormat());
    vsp.width                        = currentVSP->getWidth();
    vsp.height                       = currentVSP->getHeight();
    vsp.fps                          = currentVSP->getFps();
    /*vsp.width  = 848;
    vsp.height = 480;
    vsp.fps    = 10;*/

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

void G330NetVideoSensor::initStreamProfileList() {
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

    //Update profile list
    updateStreamProfileList(backendSpList);
}

}  // namespace libobsensor
