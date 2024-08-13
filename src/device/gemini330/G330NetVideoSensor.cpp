#include "G330NetVideoSensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "component/property/InternalProperty.hpp"
#include "utils/BufferParser.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {

G330NetVideoSensor::G330NetVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {
    initStreamProfileList();
}

void G330NetVideoSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    // TODO: SendCmd start stream by propServer
    BEGIN_TRY_EXECUTE({ VideoSensor::start(sp, callback); })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Start {} stream failed!", utils::obSensorToStr(sensorType_));
        // TODO: SendCmd stop stream by propServer
    })
}

void G330NetVideoSensor::stop() {
    VideoSensor::stop();
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
