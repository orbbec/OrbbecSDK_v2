// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VideoSensor.hpp"
#include "ISensorStreamStrategy.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "frame/Frame.hpp"
#include "FilterDecorator.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {

VideoSensor::VideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend) : SensorBase(owner, sensorType, backend) {
    auto vsPort = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    if(!vsPort) {
        throw invalid_value_exception("Backend is not a valid IVideoStreamPort");
    }

    try {
        // try to stop stream to avoid that the device is in streaming state due to some reason such as a previous crash
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to stop stream: {}", e.what());
    }

    auto backendSpList = vsPort->getStreamProfileList();
    setStreamProfileList(backendSpList);

    LOG_DEBUG("VideoSensor created @{}", sensorType_);
}

VideoSensor::~VideoSensor() noexcept {
    try {
        stop();
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while destroying VideoSensor: {}", e.what());
    }
    LOG_DEBUG("VideoSensor destroyed @{}", sensorType_);
}

#define MIN_VIDEO_FRAME_DATA_SIZE 1024
void VideoSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    LOG_INFO("Try to start stream: {}", sp);
    // validate stream profile
    {
        auto owner    = getOwner();
        auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
        if(strategy) {
            strategy->validateStream(sp);
            strategy->markStreamActivated(sp);
        }
    }

    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto backendIter = streamProfileBackendMap_.find(sp);
    if(backendIter == streamProfileBackendMap_.end()) {
        throw invalid_value_exception("Can not find backend stream profile for activated stream profile");
    }
    currentBackendStreamProfile_ = backendIter->second.first;
    currentFormatFilterConfig_   = backendIter->second.second;

    if(currentFormatFilterConfig_ && currentFormatFilterConfig_->converter) {
        auto filter          = std::dynamic_pointer_cast<FilterDecorator>(currentFormatFilterConfig_->converter);
        auto baseFilter      = filter->getBaseFilter();
        auto formatConverter = std::dynamic_pointer_cast<FormatConverter>(baseFilter);
        if(formatConverter) {
            formatConverter->setConversion(currentFormatFilterConfig_->srcFormat, currentFormatFilterConfig_->dstFormat);
        }
        currentFormatFilterConfig_->converter->setCallback([this](std::shared_ptr<Frame> frame) {
            auto deviceInfo = owner_->getInfo();
            LOG_FREQ_CALC(DEBUG, 5000, "{}({}): {} format converter frame callback, frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_,
                          sensorType_);
            outputFrame(frame);
        });
    }

    auto vsPort = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    LOG_INFO("Start backend stream: {}", currentBackendStreamProfile_);
    BEGIN_TRY_EXECUTE({
        vsPort->startStream(currentBackendStreamProfile_, [this](std::shared_ptr<Frame> frame) {  //
            onBackendFrameCallback(frame);
        });
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        {
            auto owner    = getOwner();
            auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
            if(strategy) {
                strategy->markStreamDeactivated(activatedStreamProfile_);
            }
        }
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        throw;
    })
}

void VideoSensor::onBackendFrameCallback(std::shared_ptr<Frame> frame) {
    if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
        return;
    }

    auto deviceInfo = owner_->getInfo();
    LOG_FREQ_CALC(DEBUG, 5000, "{}({}): {} backend frame callback, frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_, sensorType_);

    auto vsp              = currentBackendStreamProfile_->as<VideoStreamProfile>();
    auto maxFrameDataSize = vsp->getMaxFrameDataSize();

    auto dataSize = frame->getDataSize();
    auto format   = frame->getFormat();

#ifdef OB_DEBUG
    // auto fsp   = frame->getStreamProfile();
    // auto owner = fsp->getOwner();
    // if(fsp.get() != currentBackendStreamProfile_.get()) {
    //     throw invalid_value_exception("Frame's stream profile is not the same as activated stream profile");
    // }
    // if(owner.get() != static_cast<void *>(this)) {
    //     throw invalid_value_exception("Frame's owner is not this VideoSensor");
    // }
#endif

    if(format == OB_FORMAT_MJPG && frame->getDataSize() < MIN_VIDEO_FRAME_DATA_SIZE) {
        LOG_WARN_INTVL("This frame will be dropped because data size less than mini size (1024 byte)! size={} @{}", dataSize, sensorType_);
        return;
    }
    else if(format == OB_FORMAT_MJPG && sensorType_ != OB_SENSOR_DEPTH && !utils::checkJpgImageData(frame->getData(), dataSize)) {
        LOG_WARN_INTVL("This frame will be dropped because jpg format verification failure! @{}", sensorType_);
        return;
    }
    else if(maxFrameDataSize < dataSize) {
        LOG_WARN_INTVL("This frame will be dropped because because the data size is larger than expected! size={}, expected={} @{}", dataSize, maxFrameDataSize,
                       sensorType_);
        return;
    }
    else if(IS_FIXED_SIZE_FORMAT(format) && maxFrameDataSize != dataSize) {
        LOG_WARN_INTVL("This frame will be dropped because the data size does not match the expectation! size={}, expected={} @{}", dataSize, maxFrameDataSize,
                       sensorType_);
        return;
    }

    updateStreamState(STREAM_STATE_STREAMING);

    if(frameMetadataModifier_) {
        frameMetadataModifier_->modify(frame);
    }

    if(currentFormatFilterConfig_ && currentFormatFilterConfig_->converter) {
        currentFormatFilterConfig_->converter->pushFrame(frame);
    }
    else {
        outputFrame(frame);
    }
}

void VideoSensor::stop() {
    if(!isStreamActivated()) {
        return;
    }

    try {
        auto owner    = getOwner();
        auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
        if(strategy) {
            strategy->markStreamDeactivated(activatedStreamProfile_);
        }
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to mark stream as deactivated: {}", e.what());
    }

    updateStreamState(STREAM_STATE_STOPPING);

    auto vsPort = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    vsPort->stopStream(currentBackendStreamProfile_);

    try {
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to send stop stream vendor command: {}", e.what());
    }

    updateStreamState(STREAM_STATE_STOPPED);

    if(currentFormatFilterConfig_ && currentFormatFilterConfig_->converter) {
        currentFormatFilterConfig_->converter->reset();
    }

    if(frameProcessor_) {
        frameProcessor_->reset();
    }

    activatedStreamProfile_.reset();
    frameCallback_ = nullptr;
}

void VideoSensor::trySendStopStreamVendorCmd() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    auto propertyId = -1;
    if(propertyId == -1) {
        switch(sensorType_) {
        case OB_SENSOR_IR:
        case OB_SENSOR_IR_LEFT:
            propertyId = OB_PROP_STOP_IR_STREAM_BOOL;
            break;
        case OB_SENSOR_COLOR:
            propertyId = OB_PROP_STOP_COLOR_STREAM_BOOL;
            break;
        case OB_SENSOR_IR_RIGHT:
            propertyId = OB_PROP_STOP_IR_RIGHT_STREAM_BOOL;
            break;
        case OB_SENSOR_DEPTH:
            propertyId = OB_PROP_STOP_DEPTH_STREAM_BOOL;
            break;
        default:
            return;
        }
    }
    if(propServer->isPropertySupported(propertyId, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->setPropertyValueT<bool>(propertyId, true);
    }
}

void VideoSensor::updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) {
    if(isStreamActivated()) {
        throw wrong_api_call_sequence_exception("Can not update format filter config while streaming");
    }
    formatFilterConfigs_ = configs;
    streamProfileList_.clear();

    auto owner      = getOwner();
    auto lazySelf   = std::make_shared<LazySensor>(owner, sensorType_);
    auto streamType = utils::mapSensorTypeToStreamType(sensorType_);
    for(const auto &backendSp: backendStreamProfileList_) {
        auto format   = backendSp->getFormat();
        bool filtered = false;
        std::for_each(formatFilterConfigs_.begin(), formatFilterConfigs_.end(), [&](const FormatFilterConfig &config) {
            if(config.srcFormat != format) {
                return;
            }

            if(config.policy == FormatFilterPolicy::REMOVE) {
                filtered = true;
                return;
            }

            // FormatFilterPolicy is ADD or REPLACE, add a new stream profile with the new format
            auto sp = backendSp->clone();
            sp->setFormat(config.dstFormat);
            sp->bindOwner(lazySelf);
            sp->setType(streamType);
            streamProfileList_.push_back(sp);
            streamProfileBackendMap_[sp] = { backendSp, &config };

            filtered |= (config.policy != FormatFilterPolicy::ADD);  // if policy is REPLACE, filter out the original stream profile
        });

        if(!filtered) {  // if no filter applied, add the original stream profile
            auto sp = backendSp->clone();
            sp->bindOwner(lazySelf);
            sp->setType(streamType);
            streamProfileList_.push_back(sp);
            streamProfileBackendMap_[sp] = { backendSp, nullptr };
            continue;
        }
    }

#ifdef _DEBUG
    LOG_TRACE(" filtered stream profile list size={} @{}", streamProfileList_.size(), sensorType_);
    for(auto &sp: streamProfileList_) {
        LOG_TRACE(" - {}", sp);
    }
#endif
}

void VideoSensor::setStreamProfileList(const StreamProfileList &profileList) {
    auto              lazySelf      = std::make_shared<LazySensor>(owner_, sensorType_);
    auto              streamType    = utils::mapSensorTypeToStreamType(sensorType_);
    StreamProfileList backendSpList = profileList;
    for(auto &backendSp: backendSpList) {
        auto sp = backendSp->clone();
        sp->bindOwner(lazySelf);
        sp->setType(streamType);
        backendStreamProfileList_.push_back(sp);
        LOG_DEBUG("Backend stream profile {}", backendSp);
    }

    std::sort(backendStreamProfileList_.begin(), backendStreamProfileList_.end(),
              [](const std::shared_ptr<const StreamProfile> &a, const std::shared_ptr<const StreamProfile> &b) {
                  auto aVsp = a->as<VideoStreamProfile>();
                  auto bVsp = b->as<VideoStreamProfile>();
                  auto aRes = aVsp->getWidth() * aVsp->getHeight();
                  auto bRes = bVsp->getWidth() * bVsp->getHeight();
                  if(aRes != bRes) {
                      return aRes > bRes;
                  }
                  else if(aVsp->getHeight() != bVsp->getHeight()) {
                      return aVsp->getHeight() > bVsp->getHeight();
                  }
                  else if(aVsp->getFps() != bVsp->getFps()) {
                      return aVsp->getFps() > bVsp->getFps();
                  }
                  return aVsp->getFormat() > bVsp->getFormat();
              });

    // The stream profile list is same as the backend stream profile list at default.
    for(auto &backendSp: backendStreamProfileList_) {
        auto sp = backendSp->clone();
        sp->bindOwner(lazySelf);
        sp->setType(streamType);
        streamProfileList_.push_back(sp);
        streamProfileBackendMap_[sp] = { backendSp, nullptr };
    }

    if(!formatFilterConfigs_.empty()) {
        updateFormatFilterConfig(formatFilterConfigs_);
    }
}

void VideoSensor::setFrameMetadataModifer(std::shared_ptr<IFrameMetadataModifier> modifier) {
    if(isStreamActivated()) {
        throw wrong_api_call_sequence_exception("Can not update frame metadata modifier while streaming");
    }
    frameMetadataModifier_ = modifier;
}

}  // namespace libobsensor
