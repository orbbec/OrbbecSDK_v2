// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxProDisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "component/property/InternalProperty.hpp"
#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "frame/FrameFactory.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {

constexpr uint16_t REAL_PROFILE_WIDTH_640  = 640;
constexpr uint16_t REAL_PROFILE_HEIGHT_320 = 320;
constexpr uint16_t REAL_PROFILE_WIDTH_320  = 320;
constexpr uint16_t REAL_PROFILE_HEIGHT_160 = 160;

constexpr uint16_t VIRTUAL_PROFILE_WIDTH_640  = 640;
constexpr uint16_t VIRTUAL_PROFILE_HEIGHT_400 = 400;
constexpr uint16_t VIRTUAL_PROFILE_WIDTH_320  = 320;
constexpr uint16_t VIRTUAL_PROFILE_HEIGHT_200 = 200;

MaxProDisparitySensor::MaxProDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend), isCropStreamProfile_(false) {
    convertProfileAsDisparityBasedProfile();
}

void MaxProDisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto                                 owner          = getOwner();
    auto                                 propertyServer = owner->getPropertyServer();
    std::shared_ptr<const StreamProfile> playStreamProfile = sp;
    OBPropertyValue                      value;
    value.intValue = 1;

    isCropStreamProfile_        = false;
    realActivatedStreamProfile_ = sp;
    auto inVsp            = sp->as<const VideoStreamProfile>();
    for(const auto &entry: profileVirtualRealMap_) {
        auto virtualSp  = entry.first;
        auto virtualVsp = virtualSp->as<const VideoStreamProfile>();
        if(virtualVsp->getHeight() == inVsp->getHeight() && virtualVsp->getFps() == inVsp->getFps() && virtualVsp->getFormat() == inVsp->getFormat()) {
            playStreamProfile           = entry.second;
            realActivatedStreamProfile_ = virtualSp;
            isCropStreamProfile_        = true;
            value.intValue              = 0;
            break;
        }
    }

    BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValue(OB_PROP_DEPTH_LOAD_ENGINE_GROUP_PARAM_INT, value, PROP_ACCESS_INTERNAL); })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Max Pro depth sensor start failed!");
        throw; 
    })

    VideoSensor::start(playStreamProfile, callback);
}

void MaxProDisparitySensor::updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) {
    VideoSensor::updateFormatFilterConfig(configs);
    convertProfileAsDisparityBasedProfile();
}

void MaxProDisparitySensor::convertProfileAsDisparityBasedProfile() {
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

void MaxProDisparitySensor::initProfileVirtualRealMap() {
    StreamProfileList profileList = getStreamProfileList();

    StreamProfileList realProfileList;
    StreamProfileList virtualProfileList;
    for(const auto &streamProfile: profileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        if(vsp->getHeight() == REAL_PROFILE_HEIGHT_320 || vsp->getHeight() == REAL_PROFILE_HEIGHT_160) {
            realProfileList.push_back(vsp);
        }

        if(vsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_400 || vsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_200) {
            virtualProfileList.push_back(vsp);
        }
    }

    for(const auto &streamProfile: realProfileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        for(const auto &vStreamProfile: virtualProfileList) {
            auto virtualVsp = vStreamProfile->as<const VideoStreamProfile>();

            if(vsp->getWidth() == REAL_PROFILE_WIDTH_640 && vsp->getHeight() == REAL_PROFILE_HEIGHT_320) {
                if(virtualVsp->getWidth() == VIRTUAL_PROFILE_WIDTH_640 && virtualVsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_400
                   && vsp->getFps() == virtualVsp->getFps() && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }

            if(vsp->getWidth() == REAL_PROFILE_WIDTH_320 && vsp->getHeight() == REAL_PROFILE_HEIGHT_160) {
                if(virtualVsp->getWidth() == VIRTUAL_PROFILE_WIDTH_320 && virtualVsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_200
                   && vsp->getFps() == virtualVsp->getFps() && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }
        }
    }
}

void MaxProDisparitySensor::setDepthUnit(float unit) {
    depthUnit_ = unit;
}

void MaxProDisparitySensor::outputFrame(std::shared_ptr<Frame> frame) {
    auto vsp = frame->as<VideoFrame>();
    vsp->setPixelType(OB_PIXEL_DISPARITY);

    auto depthFrame = frame->as<DepthFrame>();
    if(depthFrame) {
        depthFrame->setValueScale(depthUnit_);
    }

    frame->setStreamProfile(activatedStreamProfile_);
    if(frameProcessor_) {
        frameProcessor_->pushFrame(frame);
    }
    else {
        SensorBase::outputFrame(frame);
    }
}

void MaxProDisparitySensor::setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor) {
    if(isStreamActivated()) {
        throw wrong_api_call_sequence_exception("Can not update frame processor while streaming");
    }
    frameProcessor_ = frameProcessor;
    frameProcessor_->setCallback([this](std::shared_ptr<Frame> frame) {
        if(isCropStreamProfile_) {
            auto streamProfile      = realActivatedStreamProfile_->clone();
            auto videoStreamProfile = streamProfile->as<const VideoStreamProfile>();
            streamProfile->setFormat(OB_FORMAT_Y16);
            frame->setStreamProfile(streamProfile);
            frame->setDataSize(videoStreamProfile->getWidth() * videoStreamProfile->getHeight() * 2);
        }

        auto deviceInfo = owner_->getInfo();
        LOG_FREQ_CALC(DEBUG, 5000, "{}({}): {} frameProcessor_ callback frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_, sensorType_);
        SensorBase::outputFrame(frame);
    });
}

}  // namespace libobsensor
