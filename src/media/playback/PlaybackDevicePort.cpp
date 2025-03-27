// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDevicePort.hpp"
#include "stream/StreamProfile.hpp"
#include "utils/PublicTypeHelper.hpp"

namespace libobsensor {

constexpr const uint64_t MAX_SLEEP_TIME_US = 24ULL * 60ULL * 60ULL * 1000000ULL;  // 24 hours

static const std::unordered_map<uint32_t, OBPropertyType> playbackPropertyMap = {
    { OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_DISPARITY_TO_DEPTH_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, OB_INT_PROPERTY },
    { OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_COLOR_WHITE_BALANCE_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_BRIGHTNESS_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_CONTRAST_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_SATURATION_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_SHARPNESS_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_HUE_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_GAMMA_INT, OB_INT_PROPERTY },
    { OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, OB_INT_PROPERTY },
    { OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_BOOL_PROPERTY },
    { OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT, OB_INT_PROPERTY },
};

PlaybackDevicePort::PlaybackDevicePort(const std::string &filePath)
    : reader_(std::make_shared<RosReader>(filePath)),
      needUpdateBaseTime_(true),
      isLooping_(true),
      playbackThread_(&PlaybackDevicePort::playbackLoop, this),
      playbackStatus_(OB_PLAYBACK_STOPPED),
      duration_(0),
      baseFrameTimestamp_(0),
      baseSystemTimestamp_(0),
      rate_(1.0) {
    duration_ = std::chrono::duration_cast<std::chrono::nanoseconds>(reader_->getDuration()).count() / playbackTimeFreq_;

    // Note:
    // 1. Any other state change to OB_PLAYBACK_PLAYING will triggers an update to the basetime
    // 2. Any other state change to OB_PLAYBACK_PLAYING no need to lock and wakeup condition variables.
    playbackStatus_.registerEnterCallback(
        OB_PLAYBACK_PLAYING,
        [this]() {
            std::unique_lock<std::mutex> lock(playbackMutex_);
            needUpdateBaseTime_ = true;
            LOG_DEBUG("Change needUpdateBaseTime_ to true");
            playbackCv_.notify_all();
        },
        true);
}

PlaybackDevicePort::~PlaybackDevicePort() {
    {
        std::unique_lock<std::mutex> lock(playbackMutex_);
        isLooping_ = false;
        playbackCv_.notify_all();
    }

    if(playbackThread_.joinable()) {
        playbackThread_.join();
    }
}

void PlaybackDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    auto sensorType = utils::mapStreamTypeToSensorType(profile->getType());
    {
        std::unique_lock<std::mutex> lock(playbackMutex_);

        auto &frameQueue = getFrameQueue(sensorType);
        if(!frameQueue->isStarted()) {
            frameQueue->start(callback);
        }
        activeSensors_.set(sensorType);
    }
    if(playbackStatus_.getCurrentState() == OB_PLAYBACK_STOPPED) {
        playbackStatus_.transitionTo(OB_PLAYBACK_PLAYING);
    }

    LOG_DEBUG("Start to play stream for sensor: {}", sensorType);
}

void PlaybackDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    auto                         sensorType = utils::mapStreamTypeToSensorType(profile->getType());
    std::unique_lock<std::mutex> lock(playbackMutex_);
    activeSensors_.reset(sensorType);
    auto freameQueue = getFrameQueue(sensorType);
    LOG_DEBUG(" - Stop to play stream for sensor: {}, ", sensorType);
    freameQueue->flush();
    LOG_DEBUG(" - Frame queue flushed for sensor: {}", sensorType);
    freameQueue->reset();
    LOG_DEBUG(" - Frame queue reset for sensor: {}", sensorType);

    if(activeSensors_.none()) {
        reader_->stop();
        playbackStatus_.transitionTo(OB_PLAYBACK_STOPPED);
    }
    playbackCv_.notify_all();
}

void PlaybackDevicePort::stopAllStream() {
    std::unique_lock<std::mutex> lock(playbackMutex_);
    activeSensors_.reset();
    playbackStatus_.transitionTo(OB_PLAYBACK_STOPPED);
    reader_->stop();

    playbackCv_.notify_all();
}

StreamProfileList PlaybackDevicePort::getStreamProfileList(OBSensorType sensorType) {
    std::vector<std::shared_ptr<const StreamProfile>> spList;

    auto profile = reader_->getStreamProfile(utils::mapSensorTypeToStreamType(sensorType));
    if(profile) {
        spList.emplace_back(profile);
    }
    else {
        LOG_DEBUG("Failed to get playback stream profile for sensor : {}", sensorType);
    }

    return spList;
}

void PlaybackDevicePort::playbackLoop() {
    while(isLooping_) {
        {
            std::unique_lock<std::mutex> lock(playbackMutex_);
            playbackCv_.wait(lock, [this]() { return !isLooping_ || (playbackStatus_.getCurrentState() == OB_PLAYBACK_PLAYING && !activeSensors_.none()); });
            if(!isLooping_) {
                break;
            }
        }

        bool isEndOfFile = reader_->getIsEndOfFile();
        if(!isEndOfFile) {
            std::shared_ptr<Frame> frame;
            try {
                frame = reader_->readNextData();
            }
            catch(const orbbecRosbag::Exception &e) {
                LOG_ERROR("Error when reading data from .bag file: {}", e.what());
                continue;
            }
            catch(const libobsensor_exception &e) {
                LOG_WARN("Error when create playback frame, message: {}", e.what());
            }

            if(!frame) {
                LOG_DEBUG("The frame is empty when reading from .bag file");
                continue;
            }

            uint64_t sleepTime = calculateSleepTime(frame->getTimeStampUsec());  // in microseconds
            if(sleepTime > 0 && sleepTime < MAX_SLEEP_TIME_US) {
                utils::sleepMs(sleepTime / 1000);  // in milliseconds
            }

            auto sensorType = utils::mapFrameTypeToSensorType(frame->getType());
            {
                std::lock_guard<std::mutex> lock(playbackMutex_);
                if(!activeSensors_.test(sensorType) || !frameQueues_.count(sensorType)) {
                    continue;
                }
            }
            auto &frameQueue = getFrameQueue(sensorType);
            frameQueue->enqueue(frame);
        }
        else {
            stopAllStream();
            LOG_DEBUG("Playing stopped...");
        }
    }
}

std::shared_ptr<FrameQueue<Frame>> &PlaybackDevicePort::getFrameQueue(OBSensorType sensorType) {
    if(frameQueues_.count(sensorType) == 0) {
        frameQueues_.insert({ sensorType, std::make_shared<FrameQueue<Frame>>(maxFrameQueueSize_) });
    }

    return frameQueues_[sensorType];
}

void PlaybackDevicePort::pause() {
    LOG_DEBUG("Start to pause playback...");
    std::unique_lock<std::mutex> lock(playbackMutex_);
    playbackStatus_.transitionTo(OB_PLAYBACK_PAUSED);
    playbackCv_.notify_all();
}

void PlaybackDevicePort::resume() {
    LOG_DEBUG("Start to resume playback...");
    playbackStatus_.transitionTo(OB_PLAYBACK_PLAYING);
}

uint64_t PlaybackDevicePort::getDuration() const {
    return duration_;
}

uint64_t PlaybackDevicePort::getPosition() const {
    auto pos = reader_->getCurTime().count() / playbackTimeFreq_;
    return pos;
}

void PlaybackDevicePort::seek(uint64_t position) {
    try {
        reader_->seekToTime(std::chrono::nanoseconds(position * playbackTimeFreq_));
        std::unique_lock<std::mutex> lock(baseTimestampMutex_);
        needUpdateBaseTime_ = true;
    }
    catch(const libobsensor_exception &e) {
        LOG_WARN("Error when seeking to position: {}, {}", position, e.what());
    }
}

uint64_t PlaybackDevicePort::calculateSleepTime(uint64_t frameTimestamp) {
    uint64_t now           = 0;
    double   frameTimeDiff = 0.0;

    // double timeDiff = 0.0;
    {
        std::unique_lock<std::mutex> lock(baseTimestampMutex_);
        now = PlaybackDevicePort::getCurrentTimenUs();

        if(!needUpdateBaseTime_ && frameTimestamp >= baseFrameTimestamp_) {
            // when rate_ change, base timestamp need to be updated
            frameTimeDiff = static_cast<double>(frameTimestamp - baseFrameTimestamp_) / static_cast<double>(rate_);
        }
        else {
            updateFrameBaseTimestamp(frameTimestamp, now);
            return 0;
        }
    }

    uint64_t sysTimeDiff      = now - baseSystemTimestamp_;
    uint64_t exceptedTimeDiff = static_cast<uint64_t>(frameTimeDiff + 0.5);  // rounded to the nearest nanosecond
    // LOG_DEBUG("sysTimeDiff:{}, frameTimeDiff:{}", sysTimeDiff, frameTimeDiff);

    return exceptedTimeDiff > sysTimeDiff ? exceptedTimeDiff - sysTimeDiff : 0;
}

void PlaybackDevicePort::updateFrameBaseTimestamp(uint64_t frameTimestamp, uint64_t sysTimestamp) {
    baseFrameTimestamp_  = frameTimestamp;
    baseSystemTimestamp_ = sysTimestamp;
    needUpdateBaseTime_  = false;
    LOG_DEBUG("update basetime, systime:{}, frametime:{}", baseSystemTimestamp_, baseFrameTimestamp_);
}

void PlaybackDevicePort::getRecordedPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    if(!value) {
        LOG_ERROR("Output value pointer is null for property {}", propertyId);
        return;
    }

    memset(value, 0, sizeof(OBPropertyValue));
    if(!playbackPropertyMap.count(propertyId)) {
        LOG_WARN("Unsupported property id for current playback device: {}", propertyId);
        return;
    }

    std::vector<uint8_t> data    = reader_->getPropertyData(propertyId);
    auto                 proType = playbackPropertyMap.at(propertyId);

    if(proType == OB_BOOL_PROPERTY || proType == OB_INT_PROPERTY) {
        memcpy(&value->intValue, data.data(), data.size());
        // LOG_DEBUG("Get recorded property value for property id: {}, get as: {}|{}", propertyId, value->intValue, value->floatValue);
    }
    else if(proType == OB_FLOAT_PROPERTY) {
        memcpy(&value->floatValue, data.data(), data.size());
    }
}

std::vector<uint8_t> PlaybackDevicePort::getRecordedStructData(uint32_t propertyId) {
    std::vector<uint8_t> data = reader_->getPropertyData(propertyId);
    if(data.empty()) {
        LOG_WARN("Failed to get recorded struct data for property id: {}", propertyId);
    }

    return data;
}

std::shared_ptr<DeviceInfo> PlaybackDevicePort::getDeviceInfo() const {
    return reader_->getDeviceInfo();
}

std::vector<OBSensorType> PlaybackDevicePort::getSensorList() const {
    return reader_->getSensorTypeList();
}

void PlaybackDevicePort::setPlaybackRate(const float &rate) {
    LOG_DEBUG("Set playback rate to {}", rate);
    std::unique_lock<std::mutex> lock(baseTimestampMutex_);
    rate_               = rate;
    needUpdateBaseTime_ = true;
}

void PlaybackDevicePort::setPlaybackStatusCallback(const PlaybackStatusCallback callback) {
    if (callback == nullptr) {
        return;
    }
    playbackStatus_.clearGlobalCallbacks();
    playbackStatusCallback_ = callback;
    playbackStatus_.registerGlobalCallback([this]() {
        LOG_DEBUG("Playback status change to {}", static_cast<int>(playbackStatus_.getCurrentState()));
        playbackStatusCallback_(playbackStatus_.getCurrentState());
    });
}

OBPlaybackStatus PlaybackDevicePort::getCurrentPlaybackStatus() const {
    return playbackStatus_.getCurrentState();
}

// For override virtual function, the following code is not implemented
void PlaybackDevicePort::startStream(MutableFrameCallback callback) {
    (void)callback;
}

void PlaybackDevicePort::stopStream() {}

StreamProfileList PlaybackDevicePort::getStreamProfileList() {
    // For interface compatibility, this interface is not implemented.
    // The stream list of the playback sensor is set after the sensor is initialized.
    return {};
}

std::shared_ptr<const SourcePortInfo> PlaybackDevicePort::getSourcePortInfo() const {
    return nullptr;
}
}  // namespace libobsensor