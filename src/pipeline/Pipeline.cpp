// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

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

void Pipeline::switchConfig(std::shared_ptr<const Config> cfg) {
    if(!cfg) {
        throw libobsensor::invalid_value_exception("Null pointer config!");
    }

    if(config_ && *cfg == *config_) {
        LOG_INFO("Noting will be execute due to pipeline config have no been changed!");
        return;
    }

    bool restartRequired = streamState_ == STREAM_STATE_STREAMING || streamState_ == STREAM_STATE_STARTING;
    if(restartRequired) {
        stopStream();
        std::unique_lock<std::mutex> lk(streamMutex_);
        start(cfg);
    }
    else {
        std::unique_lock<std::mutex> lk(streamMutex_);
        applyConfig(cfg);
    }
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
    enableHardwareD2C(false);

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
    if(!device_) {
        return d2cDepthProfileList;
    }

    auto algParamManager         = device_->getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto d2cProfileList          = algParamManager->getD2CProfileList();
    auto colorVideoStreamProfile = colorProfile->as<VideoStreamProfile>();
    auto depthSensor             = device_->getSensor(OB_SENSOR_DEPTH);
    auto depthProfiles           = depthSensor->getStreamProfileList();
    if(alignMode == ALIGN_DISABLE) {
        return depthProfiles;
    }

    for(const auto &sp: d2cProfileList) {
        if(colorVideoStreamProfile->getWidth() == sp.colorWidth && colorVideoStreamProfile->getHeight() == sp.colorHeight) {
            for(auto profile: depthProfiles) {
                auto depthProfile = profile->as<VideoStreamProfile>();
                auto width        = depthProfile->getWidth();
                auto height       = depthProfile->getHeight();
                if(sp.depthWidth == width && sp.depthHeight == height
                   && ((alignMode == ALIGN_D2C_HW_MODE && (sp.alignType & ALIGN_D2C_HW)) || (alignMode == ALIGN_D2C_SW_MODE && (sp.alignType & ALIGN_D2C_SW))))

                {
                    d2cDepthProfileList.push_back(depthProfile);
                }
            }
        }
    }

    return d2cDepthProfileList;
}

OBCameraParam Pipeline::getCameraParam() {
    OBCameraParam curCameraParam = {};
    if(!device_ || !config_) {
        return curCameraParam;
    }

    auto colorStreamProfile = config_->getEnabledStreamProfile(OB_STREAM_COLOR);
    auto depthStreamProfile = config_->getEnabledStreamProfile(OB_STREAM_DEPTH);
    if(!colorStreamProfile || !depthStreamProfile) {
        return curCameraParam;
    }

    curCameraParam.rgbIntrinsic    = colorStreamProfile->as<VideoStreamProfile>()->getIntrinsic();
    curCameraParam.rgbDistortion   = colorStreamProfile->as<VideoStreamProfile>()->getDistortion();
    curCameraParam.depthIntrinsic  = depthStreamProfile->as<VideoStreamProfile>()->getIntrinsic();
    curCameraParam.depthDistortion = depthStreamProfile->as<VideoStreamProfile>()->getDistortion();
    curCameraParam.transform       = depthStreamProfile->getExtrinsicTo(colorStreamProfile);

    return curCameraParam;
}

OBCameraParam Pipeline::getCameraParam(uint32_t colorWidth, uint32_t colorHeight, uint32_t depthWidth, uint32_t depthHeight) {
    OBCameraParam curCameraParam = {};
    if(!device_) {
        return curCameraParam;
    }
    auto colorSensor = device_->getSensor(OB_SENSOR_COLOR);
    if(!colorSensor) {
        throw invalid_value_exception(utils::string::to_string() << "No matched color sensor found");
    }
    auto depthSensor = device_->getSensor(OB_SENSOR_DEPTH);
    if(!depthSensor) {
        throw invalid_value_exception(utils::string::to_string() << "No matched depth sensor found");
    }

    auto colorSensorSpList       = colorSensor->getStreamProfileList();
    auto matchedColorProfileList = matchVideoStreamProfile(colorSensorSpList, colorWidth, colorHeight, OB_FPS_ANY, OB_FORMAT_ANY);
    if(matchedColorProfileList.empty()) {
        throw invalid_value_exception(utils::string::to_string() << "No matched color profile found");
    }
    auto colorStreamProfile = matchedColorProfileList.front();

    auto depthSensorSpList       = depthSensor->getStreamProfileList();
    auto matchedDepthProfileList = matchVideoStreamProfile(depthSensorSpList, depthWidth, depthHeight, OB_FPS_ANY, OB_FORMAT_ANY);
    if(matchedDepthProfileList.empty()) {
        throw invalid_value_exception(utils::string::to_string() << "No matched depth profile found");
    }
    auto depthStreamProfile = matchedDepthProfileList.front();

    curCameraParam.rgbIntrinsic    = colorStreamProfile->getIntrinsic();
    curCameraParam.rgbDistortion   = colorStreamProfile->getDistortion();
    curCameraParam.depthIntrinsic  = depthStreamProfile->getIntrinsic();
    curCameraParam.depthDistortion = depthStreamProfile->getDistortion();
    curCameraParam.transform       = depthStreamProfile->getExtrinsicTo(colorStreamProfile);

    return curCameraParam;
}

OBCalibrationParam Pipeline::getCalibrationParam(std::shared_ptr<Config> cfg) {
    OBCalibrationParam calibrationParam = {};
    if(!device_ || !cfg) {
        return calibrationParam;
    }

    // convert stream profile on config to real stream profile on sensors
    auto config = checkAndSetConfig(cfg);

    memset(calibrationParam.intrinsics, 0, sizeof(OBCameraIntrinsic) * OB_SENSOR_TYPE_COUNT);
    memset(calibrationParam.distortion, 0, sizeof(OBCameraDistortion) * OB_SENSOR_TYPE_COUNT);
    for(int i = 0; i < OB_SENSOR_TYPE_COUNT; i++) {
        for(int j = 0; j < OB_SENSOR_TYPE_COUNT; j++) {
            float rot[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
            memset(calibrationParam.extrinsics[i][j].trans, 0, sizeof(float) * 3);
            memcpy(calibrationParam.extrinsics[i][j].rot, rot, sizeof(float) * 9);
        }
    }

    // Intrinsic (cameras only)
    for(int i = 0; i < OB_SENSOR_TYPE_COUNT; i++) {
        auto sensorType = static_cast<OBSensorType>(i);
        if(!device_->isSensorExists(sensorType)) {
            continue;
        }
        auto streamType = utils::mapSensorTypeToStreamType(sensorType);
        if(streamType == OB_STREAM_ACCEL || streamType == OB_STREAM_GYRO) {
            continue;
        }

        auto streamProfile = config->getEnabledStreamProfile(streamType);
        if(!streamProfile) {
            auto sensor = device_->getSensor(sensorType);
            auto spList = sensor->getStreamProfileList();
            if(spList.empty()) {
                continue;
            }
            streamProfile = spList.front();
        }
        auto intrinsic  = streamProfile->as<VideoStreamProfile>()->getIntrinsic();
        auto distortion = streamProfile->as<VideoStreamProfile>()->getDistortion();
        memcpy(&calibrationParam.intrinsics[sensorType], &intrinsic, sizeof(OBCameraIntrinsic));
        memcpy(&calibrationParam.distortion[sensorType], &distortion, sizeof(OBCameraDistortion));
    }

    // Extrinsic
    for(int source = 0; source < OB_SENSOR_TYPE_COUNT; source++) {
        for(int target = 0; target < OB_SENSOR_TYPE_COUNT; target++) {
            if(source == target) {
                continue;
            }

            auto sourceSensorType = static_cast<OBSensorType>(source);
            auto targetSensorType = static_cast<OBSensorType>(target);
            if(!device_->isSensorExists(sourceSensorType) || !device_->isSensorExists(targetSensorType)) {
                continue;
            }

            auto                                 sourceStreamType = utils::mapSensorTypeToStreamType(sourceSensorType);
            auto                                 targetStreamType = utils::mapSensorTypeToStreamType(targetSensorType);
            std::shared_ptr<const StreamProfile> sourceStreamProfile;
            std::shared_ptr<const StreamProfile> targetStreamProfile;

            sourceStreamProfile = config->getEnabledStreamProfile(sourceStreamType);
            if(!sourceStreamProfile) {
                auto sensor = device_->getSensor(sourceSensorType);
                auto spList = sensor->getStreamProfileList();
                if(spList.empty()) {
                    continue;
                }
                sourceStreamProfile = spList.front();
            }
            targetStreamProfile = config->getEnabledStreamProfile(targetStreamType);
            if(!targetStreamProfile) {
                auto sensor = device_->getSensor(targetSensorType);
                auto spList = sensor->getStreamProfileList();
                if(spList.empty()) {
                    continue;
                }
                targetStreamProfile = spList.front();
            }

            auto sourceToTargetExtrinsic = sourceStreamProfile->getExtrinsicTo(targetStreamProfile);
            memcpy(&calibrationParam.extrinsics[source][target], &sourceToTargetExtrinsic, sizeof(OBExtrinsic));
            auto targetToSourceExtrinsic = targetStreamProfile->getExtrinsicTo(sourceStreamProfile);
            memcpy(&calibrationParam.extrinsics[target][source], &targetToSourceExtrinsic, sizeof(OBExtrinsic));
        }
    }

    return calibrationParam;
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

void Pipeline::checkHardwareD2CConfig() {
    auto frameProcessor      = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
    auto depthFrameProcessor = std::dynamic_pointer_cast<DepthFrameProcessor>(frameProcessor.get());
    if(!depthFrameProcessor) {
        return;
    }

    if(config_->getAlignMode() != ALIGN_D2C_HW_MODE) {
        enableHardwareD2C(false);
        return;
    }

    auto algParamManager = device_->getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto colorProfile    = config_->getEnabledStreamProfile(OB_STREAM_COLOR);
    auto depthProfile    = config_->getEnabledStreamProfile(OB_STREAM_DEPTH);
    if(algParamManager && colorProfile && depthProfile) {
        auto colorVideoStreamProfile = colorProfile->as<VideoStreamProfile>();
        auto depthVideoStreamProfile = depthProfile->as<VideoStreamProfile>();
        auto calibrationCameraParams = algParamManager->getCalibrationCameraParamList();
        auto d2cProfileList          = algParamManager->getD2CProfileList();
        depthFrameProcessor->setHardwareD2CProcessParams(colorVideoStreamProfile->getWidth(), colorVideoStreamProfile->getHeight(),
                                                         depthVideoStreamProfile->getWidth(), depthVideoStreamProfile->getHeight(), calibrationCameraParams,
                                                         d2cProfileList);
        enableHardwareD2C(true);
    }
}

void Pipeline::enableHardwareD2C(bool enable) {
    auto frameProcessor      = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
    auto depthFrameProcessor = std::dynamic_pointer_cast<DepthFrameProcessor>(frameProcessor.get());
    if(!depthFrameProcessor) {
        return;
    }
    depthFrameProcessor->enableHardwareD2CProcess(enable);
}

}  // namespace libobsensor

