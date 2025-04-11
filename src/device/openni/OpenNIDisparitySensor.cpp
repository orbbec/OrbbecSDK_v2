// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIDisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {
OpenNIDisparitySensor::OpenNIDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {
    convertProfileAsDisparityBasedProfile();
}

OpenNIDisparitySensor::~OpenNIDisparitySensor() noexcept {}

void OpenNIDisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    VideoSensor::start(sp, callback);
}

void OpenNIDisparitySensor::updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) {
    VideoSensor::updateFormatFilterConfig(configs);
    convertProfileAsDisparityBasedProfile();
}

void OpenNIDisparitySensor::convertProfileAsDisparityBasedProfile() {
    auto oldSpList = streamProfileList_;
    streamProfileList_.clear();
    for(auto &sp: oldSpList) {
        auto vsp   = sp->as<const VideoStreamProfile>();
        auto newSp = StreamProfileFactory::createDisparityBasedStreamProfile(vsp);
        auto iter = streamProfileBackendMap_.find(sp);
        streamProfileBackendMap_.insert({newSp,{iter->second.first, iter->second.second}});
        streamProfileList_.push_back(newSp);
    }
}

void OpenNIDisparitySensor::markOutputDisparityFrame(bool enable) {
    outputDisparityFrame_ = enable;
}

void OpenNIDisparitySensor::setDepthUnit(float unit) {
    depthUnit_ = unit;
}

void OpenNIDisparitySensor::outputFrame(std::shared_ptr<Frame> frame) {
    if(outputDisparityFrame_) {
        auto vsp = frame->as<VideoFrame>();
        vsp->setPixelType(OB_PIXEL_DISPARITY);
    }

    auto depthFrame = frame->as<DepthFrame>();
    if(depthFrame) {
        depthFrame->setValueScale(depthUnit_);
    }

    VideoSensor::outputFrame(frame);
}

void OpenNIDisparitySensor::setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor) {
    VideoSensor::setFrameProcessor(frameProcessor);
}

}  // namespace libobsensor
