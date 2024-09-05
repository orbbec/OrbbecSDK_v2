#pragma once

#include "StreamProfile.hpp"

namespace libobsensor {

namespace StreamProfileFactory {

std::shared_ptr<StreamProfile> createStreamProfile(OBStreamType type);
std::shared_ptr<StreamProfile> createStreamProfile(OBStreamType type, OBFormat format);

std::shared_ptr<VideoStreamProfile>          createVideoStreamProfile(OBStreamType type, OBFormat format, uint32_t width, uint32_t height, uint32_t fps);
std::shared_ptr<VideoStreamProfile>          createVideoStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width,
                                                                      uint32_t height, uint32_t fps);
std::shared_ptr<DisparityBasedStreamProfile> createDisparityBasedStreamProfile(std::shared_ptr<const VideoStreamProfile> videoProfile);

std::shared_ptr<AccelStreamProfile> createAccelStreamProfile(OBAccelFullScaleRange fullScaleRange, OBAccelSampleRate sampleRate);
std::shared_ptr<AccelStreamProfile> createAccelStreamProfile(std::shared_ptr<LazySensor> owner, OBAccelFullScaleRange fullScaleRange,
                                                             OBAccelSampleRate sampleRate);

std::shared_ptr<GyroStreamProfile> createGyroStreamProfile(OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate);
std::shared_ptr<GyroStreamProfile> createGyroStreamProfile(std::shared_ptr<LazySensor> owner, OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate);

std::shared_ptr<const StreamProfile> getStreamProfileFromEnvConfig(const std::string &nodeName, OBSensorType sensorType);
std::shared_ptr<const StreamProfile> getDefaultStreamProfileFromEnvConfig(const std::string &deviceName, OBSensorType sensorType, const std::string &tag = "");

}  // namespace StreamProfileFactory
}  // namespace libobsensor