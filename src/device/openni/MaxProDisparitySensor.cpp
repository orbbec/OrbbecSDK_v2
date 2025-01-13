// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxProDisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "component/property/InternalProperty.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {
MaxProDisparitySensor::MaxProDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {
    convertProfileAsDisparityBasedProfile();
}

void MaxProDisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto                                 owner          = getOwner();
    auto                                 propertyServer = owner->getPropertyServer();
    std::shared_ptr<const StreamProfile> playStreamProfile = sp;
    OBPropertyValue                      value;
    value.intValue = 1;

    bool foundRealProfile = false;
    auto inVsp            = sp->as<const VideoStreamProfile>();
    for(const auto &entry: profileVirtualRealMap_) {
        auto virtualSp  = entry.first;
        auto virtualVsp = virtualSp->as<const VideoStreamProfile>();
        if(virtualVsp->getHeight() == inVsp->getHeight() && virtualVsp->getFps() == inVsp->getFps() && virtualVsp->getFormat() == inVsp->getFormat()) {
            playStreamProfile = entry.second;
            foundRealProfile  = true;
            value.intValue    = 0;
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
        if(vsp->getHeight() == 320 || vsp->getHeight() == 160) {
            realProfileList.push_back(vsp);
        }

        if(vsp->getHeight() == 400 || vsp->getHeight() == 200) { 
            virtualProfileList.push_back(vsp);
        }
    }

    for(const auto &streamProfile: realProfileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        for(const auto &vStreamProfile: virtualProfileList) {
            auto virtualVsp = vStreamProfile->as<const VideoStreamProfile>();

            if(vsp->getWidth() == 640 && vsp->getHeight() == 320) {
                if(virtualVsp->getWidth() == 640 && virtualVsp->getHeight() == 400 && vsp->getFps() == virtualVsp->getFps()
                   && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }

            if(vsp->getWidth() == 320 && vsp->getHeight() == 160) {
                if(virtualVsp->getWidth() == 320 && virtualVsp->getHeight() == 200 && vsp->getFps() == virtualVsp->getFps()
                   && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }
        }
    }

}

void MaxProDisparitySensor::markOutputDisparityFrame(bool enable) {
    outputDisparityFrame_ = enable;
}

void MaxProDisparitySensor::setDepthUnit(float unit) {
    depthUnit_ = unit;
}

void MaxProDisparitySensor::outputFrame(std::shared_ptr<Frame> frame) {
    if(outputDisparityFrame_) {
        auto vsp = frame->as<VideoFrame>();
        vsp->setPixelType(OB_PIXEL_DISPARITY);
    }

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

}  // namespace libobsensor
