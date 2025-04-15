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
    : OpenNIDisparitySensor(owner, sensorType, backend), isCropStreamProfile_(false) {}

MaxProDisparitySensor::~MaxProDisparitySensor() noexcept {}

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

    OpenNIFrameProcessParam processParam = { 1, 0, 0, 0, 0, 0, 0 };
    auto it = profileProcessParamMap_.find(sp);
    if(it != profileProcessParamMap_.end()) {
        processParam = it->second;
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
    configValue      = static_cast<double>(processParam.yCut);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#7";
    configValue      = static_cast<double>(processParam.xCut);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#8";
    configValue      = static_cast<double>(processParam.ySet);
    processor->setConfigValue(configSchemaName, configValue);

    configSchemaName = "DisparityTransform#9";
    configValue      = static_cast<double>(processParam.xSet);
    processor->setConfigValue(configSchemaName, configValue);

    VideoSensor::start(playStreamProfile, callback);
}

void MaxProDisparitySensor::initProfileVirtualRealMap() {
    StreamProfileList profileList = getStreamProfileList();

    StreamProfileList realProfileList;
    StreamProfileList virtualProfileList;
    for(const auto &streamProfile: profileList) {
        auto vsp = streamProfile->as<const VideoStreamProfile>();
        OpenNIFrameProcessParam processParam = {1,0,0,0,0,0,0};
        if(vsp->getHeight() == REAL_PROFILE_HEIGHT_320 || vsp->getHeight() == REAL_PROFILE_HEIGHT_160) {
            processParam.dstWidth  = vsp->getWidth();
            processParam.dstHeight = vsp->getHeight();
            realProfileList.push_back(vsp);
        }

        if(vsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_400 || vsp->getHeight() == VIRTUAL_PROFILE_HEIGHT_200) {
            virtualProfileList.push_back(vsp);
        }
        profileProcessParamMap_[streamProfile] = processParam;
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

void MaxProDisparitySensor::setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor) {
    if(isStreamActivated()) {
        throw wrong_api_call_sequence_exception("Can not update frame processor while streaming");
    }
    frameProcessor_ = frameProcessor;
    frameProcessor_->setCallback([this](std::shared_ptr<Frame> frame) {
        auto depthFrame = frame->as<DepthFrame>();
        uint32_t dataSize = (uint32_t)depthFrame->getDataSize();
        uint32_t w        = depthFrame->getWidth();
        uint32_t h        = depthFrame->getHeight();
        LOG_DEBUG("{},{},{}", dataSize, w, h);

        auto              streamProfile     = depthFrame->getStreamProfile();
        auto              vdStreamProfile   = streamProfile->as<VideoStreamProfile>();
        OBCameraIntrinsic obCameraIntrinsic = vdStreamProfile->getIntrinsic();
        LOG_DEBUG("{}", obCameraIntrinsic.cx);
        OBCameraDistortion obCameraDistortion = vdStreamProfile->getDistortion();
        LOG_DEBUG("{}", obCameraDistortion.k1);
        OBExtrinsic obExtrinsic = vdStreamProfile->getExtrinsicTo(streamProfile);
        LOG_DEBUG("{}", obExtrinsic.rot[0]);

        auto deviceInfo = owner_->getInfo();
        LOG_FREQ_CALC(DEBUG, 5000, "{}({}): {} frameProcessor_ callback frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_, sensorType_);
        if(frameCallback_) {
            frameCallback_(frame);
        }

        LOG_FREQ_CALC(INFO, 5000, "{}({}): {} Streaming... frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_, sensorType_);
    });
}

}  // namespace libobsensor
