// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#include "Pipeline.hpp"

#include "DevicePids.hpp"
#include "context/Context.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/Utils.hpp"
#include "IAlgParamManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <cmath>
#include <algorithm>

namespace libobsensor {
Pipeline::Pipeline(std::shared_ptr<IDevice> dev) : device_(dev), config_(nullptr), streamState_(STREAM_STATE_STOPPED), pipelineCallback_(nullptr) {
    LOG_DEBUG("Pipeline init ...");
    auto sensorTypeList = device_->getSensorTypeList();
    if(sensorTypeList.empty()) {
        throw std::runtime_error("This device has no valid sensor!");
    }

    loadFrameQueueSizeConfig();

    outputFrameQueue_ = std::make_shared<FrameQueue<const Frame>>(maxFrameQueueSize_);
    frameAggregator_  = std::make_shared<FrameAggregator>();
    frameAggregator_->setCallback([&](std::shared_ptr<const Frame> frame) { outputFrame(frame); });

    TRY_EXECUTE(enableFrameSync());

    auto deviceInfo = device_->getInfo();
    LOG_INFO("Pipeline created with device: {{name: {0}, sn: {1}}}, @0x{2:X}", deviceInfo->name_, deviceInfo->deviceSn_, (uint64_t)this);
}

Pipeline::~Pipeline() noexcept {
    LOG_DEBUG("Pipeline deInit start!");

    if(streamState_ == STREAM_STATE_STARTING || streamState_ == STREAM_STATE_STREAMING) {
        TRY_EXECUTE(stop());
    }

    outputFrameQueue_->reset();
    LOG_INFO("Pipeline destroyed! @0x{:X}", (uint64_t)this);
}

void Pipeline::applyConfig(std::shared_ptr<const Config> cfg) {
    if(!cfg) {
        loadDefaultConfig();
        return;
    }
    config_ = checkAndSetConfig(cfg);
    checkHardwareD2CConfig();
}

void Pipeline::loadDefaultConfig() {
    auto config = std::make_shared<Config>();

    auto envConfig      = EnvConfig::getInstance();
    auto sensorTypeList = device_->getSensorTypeList();

    bool loaded = false;
    for(auto &sensorType: sensorTypeList) {
        auto        sensorTypeName = utils::obSensorToStr(sensorType);
        std::string nodeName       = std::string("Pipeline.Stream.") + sensorTypeName;
        if(!envConfig->isNodeContained(nodeName)) {
            continue;
        }

        std::shared_ptr<const StreamProfile> profile;

        bool UseDefaultStreamProfile = true;
        envConfig->getBooleanValue(nodeName + ".UseDefaultStreamProfile", UseDefaultStreamProfile);
        if(!UseDefaultStreamProfile) {
            profile = StreamProfileFactory::getStreamProfileFromEnvConfig(nodeName, sensorType);
        }

        if(profile) {
            config->enableStream(profile);
        }
        else {
            auto streamType = utils::mapSensorTypeToStreamType(sensorType);
            config->enableStream(streamType);
        }
        loaded = true;
    }

    if(!loaded) {
        if(std::find(sensorTypeList.begin(), sensorTypeList.end(), OB_SENSOR_DEPTH) != sensorTypeList.end()) {
            config->enableStream(OB_STREAM_DEPTH);
        }
        if(std::find(sensorTypeList.begin(), sensorTypeList.end(), OB_SENSOR_COLOR) != sensorTypeList.end()) {
            config->enableStream(OB_STREAM_COLOR);
        }
    }
    config_ = checkAndSetConfig(config);
}

void Pipeline::loadFrameQueueSizeConfig() {
    auto envConfig = EnvConfig::getInstance();

    envConfig->getIntValue("Memory.PipelineFrameQueueSize", maxFrameQueueSize_);
    if(maxFrameQueueSize_ <= 0) {
        LOG_WARN("Read xml config:pipeline frame queue size is invalid!");
        maxFrameQueueSize_ = 10;
    }

    LOG_DEBUG("loadFrameQueueSizeConfig() config queue size: {}", maxFrameQueueSize_);
}

StreamProfileList Pipeline::getEnabledStreamProfileList() {
    if(!config_) {
        return {};
    }
    return config_->getEnabledStreamProfileList();
}

std::shared_ptr<Config> Pipeline::checkAndSetConfig(std::shared_ptr<const Config> cfg) {
    LOG_DEBUG("Check and set config start!");
    std::shared_ptr<Config> config = cfg->clone();
    config->disableAllStream();
    const auto enabledStreamProfiles = cfg->getEnabledStreamProfileList();
    for(auto sp: enabledStreamProfiles) {
        auto streamType = sp->getType();
        auto sensorType = utils::mapStreamTypeToSensorType(streamType);
        auto sensor     = device_->getSensor(sensorType);
        if(!sensor) {
            throw invalid_value_exception(utils::string::to_string() << "No matched sensor found for:" << sensorType);
        }
        auto sensorSpList = sensor->getStreamProfileList();
        if(sensorType == OB_SENSOR_ACCEL) {
            auto profile            = sp->as<AccelStreamProfile>();
            auto matchedProfileList = matchAccelStreamProfile(sensorSpList, profile->getFullScaleRange(), profile->getSampleRate());
            if(matchedProfileList.empty()) {
                throw invalid_value_exception(utils::string::to_string() << "No matched profile found for:" << sp);
            }
            config->enableStream(matchedProfileList.front());
        }
        else if(sensorType == OB_SENSOR_GYRO) {
            auto profile            = sp->as<GyroStreamProfile>();
            auto matchedProfileList = matchGyroStreamProfile(sensorSpList, profile->getFullScaleRange(), profile->getSampleRate());
            if(matchedProfileList.empty()) {
                throw invalid_value_exception(utils::string::to_string() << "No matched profile found for:" << sp);
            }
            config->enableStream(matchedProfileList.front());
        }
        else {
            auto profile            = sp->as<VideoStreamProfile>();
            auto matchedProfileList = matchVideoStreamProfile(sensorSpList, profile->getWidth(), profile->getHeight(), profile->getFps(), profile->getFormat());
            if(matchedProfileList.empty()) {
                throw invalid_value_exception(utils::string::to_string() << "No matched profile found for: " << sp);
            }
            config->enableStream(matchedProfileList.front());
        }
    }
    LOG_INFO("Check and set config done!");
    return config;
}

void Pipeline::start(std::shared_ptr<const Config> cfg) {
    LOG_DEBUG("Pipeline start() start!");

    applyConfig(cfg);

    if(config_->isStreamEnabled(OB_STREAM_DEPTH) || config_->isStreamEnabled(OB_STREAM_COLOR)) {
        configAlignMode();
    }

    frameAggregator_->updateConfig(config_, true);

    streamState_ = STREAM_STATE_STARTING;
    BEGIN_TRY_EXECUTE({ startStream(); })
    CATCH_EXCEPTION_AND_EXECUTE({
        stop();
        throw;
    })

    LOG_INFO("Pipeline start done!");
}

void Pipeline::start(std::shared_ptr<const Config> cfg, FrameCallback callback) {
    pipelineCallback_ = callback;
    start(cfg);
}

std::shared_ptr<const Config> Pipeline::getConfig() {
    if(!config_) {
        loadDefaultConfig();
    }
    return config_;
}

void Pipeline::startStream() {
    LOG_INFO("Try to start streams!");

    outputFrameQueue_->reset();  // reset output frame queue before restart streams

    auto spList = config_->getEnabledStreamProfileList();
    for(const auto &sp: spList) {
        auto streamType = sp->getType();
        auto sensorType = utils::mapStreamTypeToSensorType(streamType);
        auto sensor     = device_->getSensor(sensorType);
        if(!sensor) {
            throw std::runtime_error("No sensor matched!");
        }
        sensor->start(sp, [&](std::shared_ptr<const Frame> frame) { onFrameCallback(frame); });

        LOG_DEBUG("Sensor stream started, sensorType={}", sensor->getSensorType());
    }
    LOG_INFO("Start streams done!");
}

void Pipeline::onFrameCallback(std::shared_ptr<const Frame> frame) {
    std::unique_lock<std::mutex> lk(streamMutex_);
    if(streamState_ != STREAM_STATE_STOPPED && streamState_ != STREAM_STATE_STOPPING) {
        if(streamState_ == STREAM_STATE_STARTING) {
            streamState_ = STREAM_STATE_STREAMING;
        }
        frameAggregator_->pushFrame(frame);
    }
    auto frameType = frame->getType();
    LOG_INTVL(LOG_INTVL_OBJECT_TAG + std::to_string(frameType), DEF_MIN_LOG_INTVL, spdlog::level::debug, "Frame received on pipeline! type={}", frameType);
}

void Pipeline::outputFrame(std::shared_ptr<const Frame> frame) {
    LOG_FREQ_CALC(DEBUG, 5000, "Pipeline streaming... frameset output rate={freq}fps", streamState_);
    if(streamState_ == STREAM_STATE_STREAMING) {
        if(pipelineCallback_ != nullptr) {
            pipelineCallback_(frame);
            return;
        }

        if(outputFrameQueue_->fulled()) {
            LOG_WARN_INTVL("Output frameset queue is full, drop oldest frameset!");
            outputFrameQueue_->dequeue();
        }
        outputFrameQueue_->enqueue(std::move(frame));
    }
}

std::shared_ptr<const Frame> Pipeline::waitForFrame(uint32_t timeout_ms) {
    auto frame = outputFrameQueue_->dequeue(timeout_ms);
    if(!frame) {
        LOG_WARN_INTVL("Wait for frame timeout, you can try to increase the wait time! current timeout={}", timeout_ms);
        return nullptr;
    }
    return frame;
}

void Pipeline::stopStream() {
    LOG_INFO("Try to stop streams!");
    if(!config_) {
        LOG_WARN("The config is null!");
        return;
    }

    auto cfgStreams = config_->getEnabledStreamProfileList();
    auto streamIter = cfgStreams.end();
    while(cfgStreams.size()) {
        streamIter--;
        auto streamType = (*streamIter)->getType();
        auto sensorType = utils::mapStreamTypeToSensorType(streamType);
        auto sensor     = device_->getSensor(sensorType);
        TRY_EXECUTE({ sensor->stop(); })
        LOG_INFO("Sensor stream stopped, sensorType={}", sensor->getSensorType());
        if(streamIter == cfgStreams.begin()) {
            break;
        }
    }

    LOG_INFO("Stop streams done!");
}

void Pipeline::stop() {
    LOG_INFO("Try to stop pipeline!");
    if(streamState_ != STREAM_STATE_STOPPED) {
        stopStream();
    }

    if(config_ && (config_->isStreamEnabled(OB_STREAM_DEPTH) || config_->isStreamEnabled(OB_STREAM_COLOR))) {
        resetAlignMode();
    }
    if(frameAggregator_) {
        frameAggregator_->clearAllFrameQueue();
    }

    // flush output frame queue
    outputFrameQueue_->flush();

    // clear callback
    pipelineCallback_ = nullptr;

    streamState_ = STREAM_STATE_STOPPED;
    LOG_INFO("Stop pipeline done!");
}

std::shared_ptr<IDevice> Pipeline::getDevice() {
    if(device_) {
        return device_;
    }
    return nullptr;
}

void Pipeline::configAlignMode() {
    // todo: implement this function
    // if(!config_) {
    //     return;
    // }
    // auto alignMode = config_->getAlignMode();
    // device_->configAlignMode(config_);
}

void Pipeline::resetAlignMode() {
    // todo: implement this function
    // if(device_) {
    //     device_->resetAlignMode();
    // }
}

StreamProfileList Pipeline::getD2CDepthProfileList(std::shared_ptr<const StreamProfile> colorProfile, OBAlignMode alignMode) {
    StreamProfileList d2cDepthProfileList;
    d2cDepthProfileList.clear();
    if(!device_){
        return d2cDepthProfileList;
    }

    auto algParamManager = device_->getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto d2cProfileList          = algParamManager->getD2CProfileList();
    auto              colorVideoStreamProfile  = colorProfile->as<VideoStreamProfile>();
    auto              depthSensor     = device_->getSensor(OB_SENSOR_DEPTH);
    auto              depthProfiles = depthSensor->getStreamProfileList();
    if(alignMode == ALIGN_DISABLE) {
        return depthProfiles;
    }

    for(const auto &sp: d2cProfileList) {
        if(colorVideoStreamProfile->getWidth() == sp.colorWidth && colorVideoStreamProfile->getHeight() == sp.colorHeight) {
            for(auto profile: depthProfiles) {
                auto depthProfile = profile->as<VideoStreamProfile>();
                auto width = depthProfile->getWidth();
                auto height = depthProfile->getHeight();
                if(sp.depthWidth == width && sp.depthHeight == height
                   && ((alignMode == ALIGN_D2C_HW_MODE && (sp.alignType & ALIGN_D2C_HW)) || (alignMode == ALIGN_D2C_SW_MODE && (sp.alignType &
                   ALIGN_D2C_SW))))

                {
                    d2cDepthProfileList.push_back(depthProfile);
                }
            }
        }
    }

    return d2cDepthProfileList;
}

void Pipeline::enableFrameSync() {
    if(device_->getExtensionInfo("AllSensorsUsingSameClock") == "true") {
        frameAggregator_->enableFrameSync(FrameSyncModeSyncAccordingFrameTimestamp);
    }
    else {
        LOG_WARN("Frame sync is not supported for sensors with different clocks! Use system timestamp instead, the accuracy may be lower!");
        frameAggregator_->enableFrameSync(FrameSyncModeSyncAccordingSystemTimestamp);
    }
}

void Pipeline::disableFrameSync() {
    frameAggregator_->enableFrameSync(FrameSyncModeDisable);
}

std::shared_ptr<const VideoStreamProfile> Pipeline::getCurrentVideoStreamProfile(OBStreamType type) {
    if(!config_) {
        return nullptr;
    }

    for(const auto &sp: config_->getEnabledStreamProfileList()) {
        if(sp->getType() == type) {  // todo: different type
            return sp->as<const VideoStreamProfile>();
        }
    }
    return nullptr;
}

void Pipeline::checkHardwareD2CConfig() {
    auto frameProcessor      = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
    auto depthFrameProcessor = std::dynamic_pointer_cast<DepthFrameProcessor>(frameProcessor.get());
    if(!depthFrameProcessor) {
        return;
    }
    if(config_->getAlignMode() != ALIGN_D2C_HW_MODE) {
        depthFrameProcessor->enableHardwareD2CProcess(false);
        LOG_DEBUG("current align mode is not hardware d2c mode.");
        return;
    }
    auto algParamManager = device_->getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto colorProfile    = getCurrentVideoStreamProfile(OB_STREAM_COLOR);
    auto depthProfile    = getCurrentVideoStreamProfile(OB_STREAM_DEPTH);
    if(algParamManager && colorProfile && depthProfile) {
        auto calibrationCameraParams = algParamManager->getCalibrationCameraParamList();
        auto d2cProfileList          = algParamManager->getD2CProfileList();
        depthFrameProcessor->setHardwareD2CProcessParams(colorProfile->getWidth(), colorProfile->getHeight(), depthProfile->getWidth(),
                                                         depthProfile->getHeight(), calibrationCameraParams, d2cProfileList);
        depthFrameProcessor->enableHardwareD2CProcess(true);
    }
}

}  // namespace libobsensor
