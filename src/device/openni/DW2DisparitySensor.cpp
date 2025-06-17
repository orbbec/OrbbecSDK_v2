// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DW2DisparitySensor.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "component/property/InternalProperty.hpp"
#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "frame/FrameFactory.hpp"
#include "frame/Frame.hpp"
#include "IProperty.hpp"

namespace libobsensor {

constexpr uint16_t REAL_PROFILE_WIDTH_540  = 540;
constexpr uint16_t REAL_PROFILE_HEIGHT_400 = 400;
constexpr uint16_t REAL_PROFILE_WIDTH_270  = 270;
constexpr uint16_t REAL_PROFILE_HEIGHT_200 = 200;

constexpr uint16_t VIRTUAL_PROFILE_WIDTH_640  = 640;
constexpr uint16_t VIRTUAL_PROFILE_HEIGHT_400 = 400;
constexpr uint16_t VIRTUAL_PROFILE_WIDTH_320  = 320;
constexpr uint16_t VIRTUAL_PROFILE_HEIGHT_200 = 200;

DW2DisparitySensor::DW2DisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : OpenNIDisparitySensor(owner, sensorType, backend) {}

DW2DisparitySensor::~DW2DisparitySensor() noexcept {}

void DW2DisparitySensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto                                 owner          = getOwner();
    auto                                 propertyServer = owner->getPropertyServer();
    std::shared_ptr<const StreamProfile> playStreamProfile = sp;

    auto inVsp            = sp->as<const VideoStreamProfile>();
    for(const auto &entry: profileVirtualRealMap_) {
        auto virtualSp  = entry.first;
        auto virtualVsp = virtualSp->as<const VideoStreamProfile>();
        if(virtualVsp->getHeight() == inVsp->getHeight() && virtualVsp->getFps() == inVsp->getFps() && virtualVsp->getFormat() == inVsp->getFormat()) {
            playStreamProfile           = entry.second;
            break;
        }
    }

    bool                    foundProcessParam = false;
    OpenNIFrameProcessParam processParam      = { 1, 0, 0, 0, 0, 0, 0 };
    currentProcessParam_                      = { 1, 0, 0, 0, 0, 0, 0 };
    auto it = profileProcessParamMap_.find(sp);
    if(it != profileProcessParamMap_.end()) {
        processParam = it->second;
        foundProcessParam = true;
    }

    if(!foundProcessParam) {
        if(playbackProcessParam_.size() == 1) {
            processParam = playbackProcessParam_[0];
        }
    }

    auto        processor        = getOwner()->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    std::string configSchemaName = "DisparityTransform#3";
    double      configValue      = static_cast<double>(processParam.dstWidth);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#4";
    configValue      = static_cast<double>(processParam.dstHeight);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#5";
    configValue      = static_cast<double>(processParam.scale);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#6";
    configValue      = static_cast<double>(processParam.xCut);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#7";
    configValue      = static_cast<double>(processParam.yCut);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#8";
    configValue      = static_cast<double>(processParam.ySet);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#9";
    configValue      = static_cast<double>(processParam.xSet);
    processor->setConfigValue(configSchemaName, configValue);

    currentProcessParam_ = processParam;

    VideoSensor::start(playStreamProfile, callback);
}

void DW2DisparitySensor::initProfileVirtualRealMap() {
    StreamProfileList profileList = getStreamProfileList();

    StreamProfileList realProfileList;
    StreamProfileList virtualProfileList;
    for(const auto &streamProfile: profileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        OpenNIFrameProcessParam processParam = {1,0,0,0,0,0,0};
        if(vsp->getWidth() == REAL_PROFILE_WIDTH_540 || vsp->getWidth() == REAL_PROFILE_WIDTH_270) {
            processParam.dstWidth  = vsp->getWidth();
            processParam.dstHeight = vsp->getHeight();
            processParam.xCut      = vsp->getWidth() == REAL_PROFILE_WIDTH_540 ? 50 : 25;
            realProfileList.push_back(vsp);
        }

        if(vsp->getWidth() == VIRTUAL_PROFILE_WIDTH_640 || vsp->getWidth() == VIRTUAL_PROFILE_WIDTH_320) {
            virtualProfileList.push_back(vsp);
        }
        profileProcessParamMap_[streamProfile] = processParam;
    }

    for(const auto &streamProfile: realProfileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        for(const auto &vStreamProfile: virtualProfileList) {
            auto virtualVsp = vStreamProfile->as<const VideoStreamProfile>();

            if(vsp->getWidth() == REAL_PROFILE_WIDTH_540 && vsp->getHeight() == REAL_PROFILE_HEIGHT_400) {
                if(virtualVsp->getWidth() == VIRTUAL_PROFILE_WIDTH_640 && virtualVsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_400
                   && vsp->getFps() == virtualVsp->getFps() && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }

            if(vsp->getWidth() == REAL_PROFILE_WIDTH_270 && vsp->getHeight() == REAL_PROFILE_HEIGHT_200) {
                if(virtualVsp->getWidth() == VIRTUAL_PROFILE_WIDTH_320 && virtualVsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_200
                   && vsp->getFps() == virtualVsp->getFps() && vsp->getFormat() == virtualVsp->getFormat()) {
                    profileVirtualRealMap_[streamProfile] = vStreamProfile;
                }
            }
        }
    }
}

}  // namespace libobsensor
