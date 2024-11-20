// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SensorBase.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "frame/Frame.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerHelper.hpp"

namespace libobsensor {
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
      streamInterruptTimeoutMs_(DefaultStreamInterruptTimeoutMs) {}

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
    auto                                 aa = profile->as<VideoStreamProfile>();
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
    auto curSp    = activatedStreamProfile_;
    auto callback = frameCallback_;
    stop();
    start(curSp, callback);
}

void SensorBase::updateStreamState(OBStreamState state) {
    std::unique_lock<std::mutex> lock(streamStateMutex_);
    if(onRecovering_) {
        return;
    }
    auto oldState = streamState_.load();
    streamState_.store(state);
    if(oldState != state && !streamStateChangedCallbacks_.empty()) {
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
        }
    }
    if(oldState != state) {
        LOG_DEBUG("Stream state changed to {}@{}", STREAM_STATE_STR(state), sensorType_);
    }
    streamStateCv_.notify_all();
}

void SensorBase::enableStreamRecovery(uint32_t maxRecoveryCount, int noStreamTimeoutMs, int streamInterruptTimeoutMs) {
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

void SensorBase::disableStreamRecovery() {
    recoveryEnabled_ = false;
    streamStateCv_.notify_all();
    if(streamStateWatcherThread_.joinable()) {
        streamStateWatcherThread_.join();
    }
}

void SensorBase::watchStreamState() {
    recoveryCount_ = 0;
    while(recoveryEnabled_) {
        if(streamState_ == STREAM_STATE_STOPPED || streamState_ == STREAM_STATE_STOPPING || streamState_ == STREAM_STATE_ERROR) {
            std::unique_lock<std::mutex> lock(streamStateMutex_);
            streamStateCv_.wait(lock);
            recoveryCount_ = 0;
        }
        else if(streamState_ == STREAM_STATE_STARTING && noStreamTimeoutMs_ > 0) {
            {
                std::unique_lock<std::mutex> lock(streamStateMutex_);
                streamStateCv_.wait_for(lock, std::chrono::milliseconds(noStreamTimeoutMs_));
                if(streamState_ != STREAM_STATE_STARTING || recoveryEnabled_ == false) {
                    recoveryCount_ = 0;
                    continue;
                }
            }
            if(recoveryCount_ < maxRecoveryCount_) {
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
                if(sts != std::cv_status::timeout || streamState_ != STREAM_STATE_STREAMING || recoveryEnabled_ == false) {
                    recoveryCount_ = 0;
                    continue;
                }
            }
            if(recoveryCount_ < maxRecoveryCount_) {
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

void SensorBase::outputFrame(std::shared_ptr<Frame> frame) {
    if(frameMetadataParserContainer_) {
        TRY_EXECUTE(frame->registerMetadataParsers(frameMetadataParserContainer_));
    }

    if(frameTimestampCalculator_) {
        TRY_EXECUTE(frameTimestampCalculator_->calculate(frame));
    }

    if(globalTimestampCalculator_) {
        TRY_EXECUTE(globalTimestampCalculator_->calculate(frame));
    }

    frameCallback_(frame);

    auto deviceInfo = owner_->getInfo();
    LOG_FREQ_CALC(INFO, 5000, "{}({}): {} Streaming... frameRate={freq}fps", deviceInfo->name_, deviceInfo->deviceSn_, sensorType_);
}

}  // namespace libobsensor
