// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SensorBase.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "frame/Frame.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "environment/EnvConfig.hpp"
#include "IDevice.hpp"
#include "IDepthWorkModeManager.hpp"

#include "logger/LoggerSnWrapper.hpp"  // Must be included last to override log macros

namespace libobsensor {

#define GetCurrentSN() owner_->getSn()

SensorBase::SensorBase(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : owner_(owner),
      sensorType_(sensorType),
      backend_(backend),
      streamState_(STREAM_STATE_STOPPED),
      onRecovering_(false),
      recoveryEnabled_(false),
      maxRecoveryCount_(DefaultMaxRecoveryCount),
      recoveryCount_(0),
      noStreamTimeoutMs_(DefaultNoStreamTimeoutMs),
      streamInterruptTimeoutMs_(DefaultStreamInterruptTimeoutMs) {
    enableTimestampAnomalyDetection(true);
    startStreamRecovery();

    auto activityRecorder = owner->getComponentT<IDeviceActivityRecorder>(OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER, false);
    if(activityRecorder) {
        deviceActivityRecorder_ = activityRecorder.get();
    }
}

SensorBase::~SensorBase() noexcept {
    if(streamStateWatcherThread_.joinable()) {
        recoveryEnabled_ = false;
        streamStateCv_.notify_all();
        streamStateWatcherThread_.join();
    }
    LOG_INFO("SensorBase is destroyed");
}

OBSensorType SensorBase::getSensorType() const {
    return sensorType_;
}

IDevice *SensorBase::getOwner() const {
    return owner_;
}

std::shared_ptr<ISourcePort> SensorBase::getBackend() const {
    return backend_;
}

OBStreamState SensorBase::getStreamState() const {
    return streamState_.load();
}

bool SensorBase::isStreamActivated() const {
    return streamState_ == STREAM_STATE_STARTING || streamState_ == STREAM_STATE_STREAMING || streamState_ == STREAM_STATE_ERROR
           || streamState_ == STREAM_STATE_STOPPING;
}

uint32_t SensorBase::registerStreamStateChangedCallback(StreamStateChangedCallback callback) {
    std::unique_lock<std::mutex> lock(streamStateCallbackMutex_);
    uint32_t                     token  = StreamStateChangedCallbackTokenCounter_++;
    streamStateChangedCallbacks_[token] = callback;
    return token;
}

void SensorBase::unregisterStreamStateChangedCallback(uint32_t token) {
    std::unique_lock<std::mutex> lock(streamStateCallbackMutex_);
    streamStateChangedCallbacks_.erase(token);
}

StreamProfileList SensorBase::getStreamProfileList() const {
    auto spList = streamProfileList_;
    if(streamProfileFilter_) {
        spList = streamProfileFilter_->filter(spList);
    }
    return spList;
}

void SensorBase::setStreamProfileFilter(std::shared_ptr<IStreamProfileFilter> filter) {
    streamProfileFilter_ = filter;
}

void SensorBase::setStreamProfileList(const StreamProfileList &profileList) {
    streamProfileList_ = profileList;
}

void SensorBase::updateDefaultStreamProfile(const std::shared_ptr<const StreamProfile> &profile) {
    std::shared_ptr<const StreamProfile> defaultProfile;
    LOG_DEBUG("Set default stream profile to: {}", profile);
    for(auto iter = streamProfileList_.begin(); iter != streamProfileList_.end(); ++iter) {
        if((*iter)->is<VideoStreamProfile>() && profile->is<VideoStreamProfile>()) {
            auto vsp    = (*iter)->as<VideoStreamProfile>();
            auto vspCmp = profile->as<VideoStreamProfile>();
            if(vsp->getFormat() == vspCmp->getFormat() && vsp->getWidth() == vspCmp->getWidth() && vsp->getHeight() == vspCmp->getHeight()
               && vsp->getFps() == vspCmp->getFps()) {
                defaultProfile = *iter;
                streamProfileList_.erase(iter);
                break;
            }
        }
        else if((*iter)->is<AccelStreamProfile>() && profile->is<AccelStreamProfile>()) {
            auto asp    = (*iter)->as<AccelStreamProfile>();
            auto aspCmp = profile->as<AccelStreamProfile>();
            if(asp->getFullScaleRange() == aspCmp->getFullScaleRange() && asp->getSampleRate() == aspCmp->getSampleRate()) {
                defaultProfile = *iter;
                streamProfileList_.erase(iter);
                break;
            }
        }
        else if((*iter)->is<GyroStreamProfile>() && profile->is<GyroStreamProfile>()) {
            auto gsp    = (*iter)->as<GyroStreamProfile>();
            auto gspCmp = profile->as<GyroStreamProfile>();
            if(gsp->getFullScaleRange() == gspCmp->getFullScaleRange() && gsp->getSampleRate() == gspCmp->getSampleRate()) {
                defaultProfile = *iter;
                streamProfileList_.erase(iter);
                break;
            }
        }
        else if((*iter)->is<LiDARStreamProfile>() && profile->is<LiDARStreamProfile>()) {
            auto lsp    = (*iter)->as<LiDARStreamProfile>();
            auto lspCmp = profile->as<LiDARStreamProfile>();
            if(lsp->getFormat() == lspCmp->getFormat() && lsp->getScanRate() == lspCmp->getScanRate()) {
                defaultProfile = *iter;
                streamProfileList_.erase(iter);
                break;
            }
        }
    }

    if(defaultProfile) {
        // insert the default profile at the beginning of the list
        streamProfileList_.insert(streamProfileList_.begin(), defaultProfile);
    }
    else {
        LOG_WARN("Failed to update default stream profile for sensor due to no matching stream profile found");
    }
}

std::shared_ptr<const StreamProfile> SensorBase::getActivatedStreamProfile() const {
    return activatedStreamProfile_;
}

FrameCallback SensorBase::getFrameCallback() const {
    return frameCallback_;
}

void SensorBase::restartStream() {
    VALIDATE_NOT_NULL(activatedStreamProfile_);
    auto curSp    = activatedStreamProfile_;
    auto callback = frameCallback_;
    stop();
    BEGIN_TRY_EXECUTE(start(curSp, callback);)
    CATCH_EXCEPTION_AND_EXECUTE({
        activatedStreamProfile_ = curSp;
        frameCallback_          = callback;
        LOG_ERROR("Failed to restart stream");
        throw;
    });
}

void SensorBase::updateStreamState(OBStreamState state) {
    std::unique_lock<std::mutex> lock(streamStateMutex_);
    if(onRecovering_) {
        return;
    }
    auto oldState = streamState_.load();
    if(state == STREAM_STATE_STREAMING && oldState == STREAM_STATE_STOPPING) {
        return;
    }

    streamState_.store(state);
    if(oldState != state) {
        for(auto &callback: streamStateChangedCallbacks_) {
            callback.second(state, activatedStreamProfile_);  // call the callback function
        }
        if(state == STREAM_STATE_STARTING) {
            if(frameTimestampCalculator_) {
                frameTimestampCalculator_->clear();
            }
            if(globalTimestampCalculator_) {
                globalTimestampCalculator_->clear();
            }

            if(timestampAnomalyDetector_) {
                timestampAnomalyDetector_->clear();
                uint32_t fps = 0;
                if(activatedStreamProfile_->is<VideoStreamProfile>()) {
                    fps = activatedStreamProfile_->as<VideoStreamProfile>()->getFps();
                }
                else if(activatedStreamProfile_->is<AccelStreamProfile>()) {
                    fps = static_cast<uint32_t>(utils::mapIMUSampleRateToValue(activatedStreamProfile_->as<AccelStreamProfile>()->getSampleRate()));
                }
                else if(activatedStreamProfile_->is<GyroStreamProfile>()) {
                    fps = static_cast<uint32_t>(utils::mapIMUSampleRateToValue(activatedStreamProfile_->as<GyroStreamProfile>()->getSampleRate()));
                }
                timestampAnomalyDetector_->setCurrentFps(fps);
            }
        }
    }
    if(oldState != state) {
        LOG_DEBUG("Stream state changed to {}@{}", STREAM_STATE_STR(state), sensorType_);
    }
    streamStateCv_.notify_all();
}

void SensorBase::enableStreamRecovery(uint32_t maxRecoveryCount, int noStreamTimeoutMs, int streamInterruptTimeoutMs) {
    if(owner_->isPlaybackDevice()) {
        LOG_WARN("Sensor {}: the current device is a playback device and does not support stream recovery", sensorType_);
        return;
    }

    {
        std::unique_lock<std::mutex> lock(streamStateMutex_);
        recoveryCount_            = 0;
        recoveryEnabled_          = true;
        maxRecoveryCount_         = maxRecoveryCount == 0 ? maxRecoveryCount_ : maxRecoveryCount;
        noStreamTimeoutMs_        = noStreamTimeoutMs == 0 ? noStreamTimeoutMs_ : noStreamTimeoutMs;
        streamInterruptTimeoutMs_ = streamInterruptTimeoutMs == 0 ? streamInterruptTimeoutMs_ : streamInterruptTimeoutMs;
    }
    if(streamStateWatcherThread_.joinable()) {
        return;
    }
    streamStateWatcherThread_ = std::thread([this]() { watchStreamState(); });
}

void SensorBase::startStreamRecovery() {
    if(owner_->isPlaybackDevice()) {
        LOG_WARN("Sensor {}: the current device is a playback device and does not support stream recovery", sensorType_);
        return;
    }

    auto        envConfig    = EnvConfig::getInstance();
    std::string nodePathName = "Device." + getOwner()->getInfo()->name_ + ".";
    auto nodePath = nodePathName + utils::obSensorToStr(getSensorType());
    nodePath      = utils::string::removeSpace(nodePath);
    int streamFailedRetry = 0;
    int maxStartStreamDelayMs = 0;
    int  maxFrameIntervalMs = 0;
    if(envConfig->getIntValue(nodePath + ".StreamFailedRetry", streamFailedRetry) &&
        envConfig->getIntValue(nodePath + ".MaxStartStreamDelayMs", maxStartStreamDelayMs) &&
        envConfig->getIntValue(nodePath + ".MaxFrameIntervalMs", maxFrameIntervalMs)) {  
        LOG_DEBUG(" Recovery config found for sensor: {}", utils::obSensorToStr(sensorType_));   
        LOG_DEBUG(" StreamFailedRetry: {}, MaxStartStreamDelayMs: {}, MaxFrameIntervalMs: {}", streamFailedRetry, maxStartStreamDelayMs, maxFrameIntervalMs);               
        if(streamFailedRetry > 0) {
            enableStreamRecovery(streamFailedRetry, maxStartStreamDelayMs, maxFrameIntervalMs);
        }
    } else {
        LOG_INFO(" No recovery config found for sensor: {}", utils::obSensorToStr(sensorType_));
    }
}

void SensorBase::disableStreamRecovery() {
    recoveryEnabled_ = false;
    streamStateCv_.notify_all();
    if(streamStateWatcherThread_.joinable()) {
        streamStateWatcherThread_.join();
    }
}

void SensorBase::waitRecoveringFinished() {
    std::unique_lock<std::recursive_mutex> lock(streamRecoverMutex_);
}

void SensorBase::watchStreamState() {
    recoveryCount_ = 0;
    while(recoveryEnabled_) {
        bool isTriggeringMode = false;
        if((streamState_ == STREAM_STATE_STARTING && noStreamTimeoutMs_ > 0) || (streamState_ == STREAM_STATE_STREAMING && streamInterruptTimeoutMs_ > 0)) {
            BEGIN_TRY_EXECUTE(auto configurator =
                            getOwner()->getComponentT<libobsensor::IDeviceSyncConfigurator>(libobsensor::OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR);
                        auto oBMultiDeviceSyncConfig = configurator->getSyncConfig();
                        if(oBMultiDeviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING
                           || oBMultiDeviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING) {
                            isTriggeringMode = true;
                            LOG_DEBUG_INTVL_MS(5000, "{} Curent mode is not supported stream recovery", sensorType_);
                        })
            CATCH_EXCEPTION_AND_EXECUTE(
                recoveryEnabled_ = false;
                streamStateCv_.notify_all();
            )
        }

        if(streamState_ == STREAM_STATE_STOPPED || streamState_ == STREAM_STATE_STOPPING || streamState_ == STREAM_STATE_ERROR) {
            std::unique_lock<std::mutex> lock(streamStateMutex_);
            streamStateCv_.wait(lock);
            recoveryCount_ = 0;
        }
        else if(streamState_ == STREAM_STATE_STARTING && noStreamTimeoutMs_ > 0) {
            {
                std::unique_lock<std::mutex> lock(streamStateMutex_);
                streamStateCv_.wait_for(lock, std::chrono::milliseconds(noStreamTimeoutMs_));
                if(streamState_ != STREAM_STATE_STARTING || recoveryEnabled_ == false || isTriggeringMode) {
                    recoveryCount_ = 0;
                    continue;
                }
            }
            if(recoveryCount_ < maxRecoveryCount_) {
                LOG_INFO("Start stream for {} sensor failed, retrying...", utils::obSensorToStr(sensorType_));
                std::unique_lock<std::recursive_mutex> lock(streamRecoverMutex_);
                onRecovering_ = true;
                TRY_EXECUTE(restartStream());
                recoveryCount_++;
                onRecovering_ = false;
                continue;
            }
            updateStreamState(STREAM_STATE_ERROR);
            LOG_ERROR("Failed to start stream for sensor after {} retries", maxRecoveryCount_);
        }
        else if(streamState_ == STREAM_STATE_STREAMING && streamInterruptTimeoutMs_ > 0) {
            {
                std::unique_lock<std::mutex> lock(streamStateMutex_);
                auto                         sts = streamStateCv_.wait_for(lock, std::chrono::milliseconds(streamInterruptTimeoutMs_));
                if(sts != std::cv_status::timeout || streamState_ != STREAM_STATE_STREAMING || recoveryEnabled_ == false || isTriggeringMode) {
                    recoveryCount_ = 0;
                    continue;
                }
            }
            if(recoveryCount_ < maxRecoveryCount_) {
                LOG_INFO("Stream interrupted for {} sensor, retrying...", utils::obSensorToStr(sensorType_));
                std::unique_lock<std::recursive_mutex> lock(streamRecoverMutex_);
                onRecovering_ = true;
                TRY_EXECUTE(restartStream());
                recoveryCount_++;
                onRecovering_ = false;
                continue;
            }
            updateStreamState(STREAM_STATE_ERROR);
            LOG_ERROR("Failed to recover stream for sensor after {} retries", maxRecoveryCount_);
        }
    }
}

void SensorBase::setFrameMetadataParserContainer(std::shared_ptr<IFrameMetadataParserContainer> container) {
    frameMetadataParserContainer_ = container;
}

void SensorBase::setFrameTimestampCalculator(std::shared_ptr<IFrameTimestampCalculator> calculator) {
    frameTimestampCalculator_ = calculator;
}

void SensorBase::setGlobalTimestampCalculator(std::shared_ptr<IFrameTimestampCalculator> calculator) {
    globalTimestampCalculator_ = calculator;
}

void SensorBase::setFrameRecordingCallback(FrameCallback callback) {
    frameRecordingCallback_ = callback;
}

void SensorBase::setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor) {
    if(isStreamActivated()) {
        throw wrong_api_call_sequence_exception("Can not update frame processor while streaming");
    }
    frameProcessor_ = frameProcessor;
    frameProcessor_->setCallback([this](std::shared_ptr<Frame> frame) {
        LOG_FREQ_CALC(DEBUG, 5000, "{} frameProcessor_ callback frameRate={freq}fps", sensorType_);
        if (frameCallback_)
        {
            frameCallback_(frame);
        }

        LOG_FREQ_CALC(INFO, 5000, "{} Streaming... frameRate={freq}fps", sensorType_);
    });
}

void SensorBase::enableTimestampAnomalyDetection(bool enable){
    if(enable) {
        if(owner_->isPlaybackDevice()) {
            LOG_WARN("Sensor {}: the current device is a playback device and does not support timestamp anomaly detection", sensorType_);
            return;
        }
        if(!timestampAnomalyDetector_) {
            TRY_EXECUTE({ timestampAnomalyDetector_ = std::make_shared<TimestampAnomalyDetector>(owner_); });
        }
    }
    else {
        timestampAnomalyDetector_.reset();
    }
}

void SensorBase::outputFrame(std::shared_ptr<Frame> frame) {
    if(deviceActivityRecorder_) {
        // update device activity
        deviceActivityRecorder_->touch(DeviceActivity::Stream);
    }

    if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
        return;
    }

    if(activatedStreamProfile_) {
        frame->setStreamProfile(activatedStreamProfile_);
    }
    if(frameMetadataParserContainer_) {
        TRY_EXECUTE(frame->registerMetadataParsers(frameMetadataParserContainer_));
    }
    if(frameTimestampCalculator_) {
        TRY_EXECUTE(frameTimestampCalculator_->calculate(frame));
    }

    if(globalTimestampCalculator_) {
        TRY_EXECUTE(globalTimestampCalculator_->calculate(frame));
    }

    if(timestampAnomalyDetector_) {
        BEGIN_TRY_EXECUTE({ timestampAnomalyDetector_->calculate(frame); })
        CATCH_EXCEPTION_AND_EXECUTE({
            LOG_ERROR("Timestamp anomaly detected, frame: {}, sensor: {}", frame->getTimeStampUsec(), utils::obSensorToStr(sensorType_));
            return;
        });
    }

    if (frameRecordingCallback_) {
        frameRecordingCallback_(frame);
    }
    
    if(frameProcessor_) {
        frameProcessor_->pushFrame(frame);
    }
    else {
        if (frameCallback_)
        {
            frameCallback_(frame);
        }
        LOG_FREQ_CALC(INFO, 5000, "{} Streaming... frameRate={freq}fps", sensorType_);
    }
}

void SensorBase::validateDeviceState(const std::shared_ptr<const StreamProfile> &profile) {
    auto device = getOwner();

    // check if device firmware upgrade is in progress
    if(device->isFirmwareUpdating()) {
        throw libobsensor::wrong_api_call_sequence_exception("Device firmware is currently upgrading, stream cannot be started now!");
    }

    // check device error state
    auto errorState = device->getDeviceErrorState();
    if(errorState != 0) {
        uint64_t flag = 0;

        // add flag to be checked
        auto streamType = profile->getType();
        switch(streamType) {
        case OB_STREAM_COLOR:
            flag |= OB_ERROR_RGB_SENSOR;
            break;
        case OB_STREAM_ACCEL:
        case OB_STREAM_GYRO:
            flag |= OB_ERROR_IMU_SENSOR;
            break;
        case OB_STREAM_DEPTH:
        case OB_STREAM_IR:
        case OB_STREAM_IR_LEFT:
        case OB_STREAM_IR_RIGHT: {
            // Skip device error state if the depth work mode is Factory Calibration
            auto depthWorkModeManager = device->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, false);
            if(depthWorkModeManager) {
                auto        currentDepthMode = depthWorkModeManager->getCurrentDepthWorkMode();
                const char *factoryMode      = "Factory Calib";
                if(strncmp(currentDepthMode.name, factoryMode, strlen(factoryMode) + 1) == 0) {
                    // Factory Calibration mode
                    flag = 0;
                    break;
                }
            }
            flag |= OB_ERROR_IRL_SENSOR | OB_ERROR_IRR_SENSOR | OB_ERROR_CFG_PARAM;
        } break;
        default:
            break;
        }

        if((errorState & flag) != 0) {
            throw unsupported_operation_exception(utils::string::to_string() << "Unexpected device state: " << errorState
                                                                             << ". Please update your camera firmware before streaming data.");
        }
    }
}

}  // namespace libobsensor
