// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DisparityBasedSensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {
DisparityBasedSensor::DisparityBasedSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {
    convertProfileAsDisparityBasedProfile();
}

void DisparityBasedSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    VideoSensor::start(sp, callback);
}

void DisparityBasedSensor::stop() {
    VideoSensor::stop();
}

void DisparityBasedSensor::updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) {
    VideoSensor::updateFormatFilterConfig(configs);
    convertProfileAsDisparityBasedProfile();
}

void DisparityBasedSensor::setStreamProfileList(const StreamProfileList &profileList) {
    VideoSensor::setStreamProfileList(profileList);
    convertProfileAsDisparityBasedProfile();
}

void DisparityBasedSensor::convertProfileAsDisparityBasedProfile() {
    auto oldSpList = streamProfileList_;
    streamProfileList_.clear();
    for(auto &sp: oldSpList) {
        auto vsp   = sp->as<const VideoStreamProfile>();
        auto newSp = StreamProfileFactory::createDisparityBasedStreamProfile(vsp);
        auto iter  = streamProfileBackendMap_.find(sp);
        streamProfileBackendMap_.insert({ newSp, { iter->second.first, iter->second.second } });
        streamProfileList_.push_back(newSp);
    }
}

void DisparityBasedSensor::markOutputDisparityFrame(bool enable) {
    outputDisparityFrame_ = enable;
}

void DisparityBasedSensor::setDepthUnit(float unit) {
    depthUnit_ = unit;
}

void DisparityBasedSensor::outputFrame(std::shared_ptr<Frame> frame) {
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
}  // namespace libobsensor
