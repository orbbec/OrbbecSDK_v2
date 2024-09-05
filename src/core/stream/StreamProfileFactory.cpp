
#include "StreamProfileFactory.hpp"
#include "StreamIntrinsicsManager.hpp"
#include "StreamExtrinsicsManager.hpp"
#include "exception/ObException.hpp"
#include "environment/EnvConfig.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
namespace StreamProfileFactory {

std::shared_ptr<StreamProfile> createStreamProfile(OBStreamType streamType) {
    return createStreamProfile(streamType, OB_FORMAT_ANY);
}

std::shared_ptr<StreamProfile> createStreamProfile(OBStreamType streamType, OBFormat frameFormat) {
    switch(streamType) {
    case OB_STREAM_ACCEL:
        return createAccelStreamProfile(OB_ACCEL_FULL_SCALE_RANGE_ANY, OB_ACCEL_SAMPLE_RATE_ANY);
    case OB_STREAM_GYRO:
        return createGyroStreamProfile(OB_GYRO_FULL_SCALE_RANGE_ANY, OB_GYRO_SAMPLE_RATE_ANY);
    case OB_STREAM_VIDEO:
    case OB_STREAM_DEPTH:
    case OB_STREAM_COLOR:
    case OB_STREAM_IR:
    case OB_STREAM_IR_LEFT:
    case OB_STREAM_IR_RIGHT:
    case OB_STREAM_RAW_PHASE:
        return createVideoStreamProfile(streamType, frameFormat, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);

    default:
        return std::make_shared<StreamProfile>(std::shared_ptr<LazySensor>(), streamType, frameFormat);
    }
}

std::shared_ptr<VideoStreamProfile> createVideoStreamProfile(OBStreamType type, OBFormat format, uint32_t width, uint32_t height, uint32_t fps) {
    return createVideoStreamProfile(std::shared_ptr<LazySensor>(), type, format, width, height, fps);
}

std::shared_ptr<VideoStreamProfile> createVideoStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width,
                                                             uint32_t height, uint32_t fps) {
    return std::make_shared<VideoStreamProfile>(owner, type, format, width, height, fps);
}

std::shared_ptr<DisparityBasedStreamProfile> createDisparityBasedStreamProfile(std::shared_ptr<const VideoStreamProfile> videoProfile) {
    auto vsp           = std::make_shared<DisparityBasedStreamProfile>(videoProfile->getOwner(), videoProfile->getType(), videoProfile->getFormat(),
                                                                       videoProfile->getWidth(), videoProfile->getHeight(), videoProfile->getFps());
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    if(intrinsicsMgr->containsVideoStreamIntrinsics(videoProfile)) {
        auto intrinsics = intrinsicsMgr->getVideoStreamIntrinsics(videoProfile);
        vsp->bindIntrinsic(intrinsics);
    }
    if(intrinsicsMgr->containsVideoStreamDistortion(videoProfile)) {
        auto distortion = intrinsicsMgr->getVideoStreamDistortion(videoProfile);
        vsp->bindDistortion(distortion);
    }
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    extrinsicsMgr->registerSameExtrinsics(vsp, videoProfile);
    return vsp;
}

std::shared_ptr<AccelStreamProfile> createAccelStreamProfile(OBAccelFullScaleRange fullScaleRange, OBAccelSampleRate sampleRate) {
    return createAccelStreamProfile(std::shared_ptr<LazySensor>(), fullScaleRange, sampleRate);
}

std::shared_ptr<AccelStreamProfile> createAccelStreamProfile(std::shared_ptr<LazySensor> owner, OBAccelFullScaleRange fullScaleRange,
                                                             OBAccelSampleRate sampleRate) {
    auto asp = std::make_shared<AccelStreamProfile>(owner, fullScaleRange, sampleRate);
    return asp;
}

std::shared_ptr<GyroStreamProfile> createGyroStreamProfile(OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate) {
    return createGyroStreamProfile(std::shared_ptr<LazySensor>(), fullScaleRange, sampleRate);
}

std::shared_ptr<GyroStreamProfile> createGyroStreamProfile(std::shared_ptr<LazySensor> owner, OBGyroFullScaleRange fullScaleRange,
                                                           OBGyroSampleRate sampleRate) {
    auto gsp = std::make_shared<GyroStreamProfile>(owner, fullScaleRange, sampleRate);
    return gsp;
}

std::shared_ptr<const StreamProfile> getStreamProfileFromEnvConfig(const std::string &nodePathName, OBSensorType sensorType) {
    auto envConfig = EnvConfig::getInstance();

    auto nodePath = nodePathName + utils::obSensorToStr(sensorType);
    nodePath      = utils::string::removeSpace(nodePath);

    if(sensorType == OB_SENSOR_ACCEL || sensorType == OB_SENSOR_GYRO) {
        return nullptr;  // todo: implement it
    }

    // video stream profile
    int         w = 0, h = 0, fps = 0;
    std::string fmt;
    if(envConfig->getIntValue(nodePath + ".Width", w) && envConfig->getIntValue(nodePath + ".Height", h) && envConfig->getIntValue(nodePath + ".FPS", fps)
       && envConfig->getStringValue(nodePath + ".Format", fmt)) {
        auto width      = static_cast<uint32_t>(w);
        auto height     = static_cast<uint32_t>(h);
        auto frameRate  = static_cast<uint32_t>(fps);
        auto format     = utils::strToOBFormat(fmt);
        auto streamType = utils::mapSensorTypeToStreamType(sensorType);
        return StreamProfileFactory::createVideoStreamProfile(streamType, format, width, height, frameRate);
    }

    return nullptr;
}

std::shared_ptr<const StreamProfile> getDefaultStreamProfileFromEnvConfig(const std::string &deviceName, OBSensorType sensorType, const std::string &tag) {
    auto        envConfig    = EnvConfig::getInstance();
    std::string nodePathName = "Device." + deviceName + ".";
    if(!tag.empty()) {
        nodePathName += tag;
        nodePathName += ".";
    }
    return getStreamProfileFromEnvConfig(nodePathName, sensorType);
}

}  // namespace StreamProfileFactory
}  // namespace libobsensor