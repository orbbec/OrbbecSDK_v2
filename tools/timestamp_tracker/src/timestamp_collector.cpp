// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "timestamp_collector.hpp"
#include "utils.hpp"
#include <cmath>
#include <chrono>
#include <iostream>

namespace libobsensor {
namespace tools {

TimestampCollector::TimestampCollector(std::shared_ptr<ob::Device> device, OBClockType clockType) : device_(device), clockType_(clockType) {
    auto devInfo  = device_->getDeviceInfo();
    deviceName_   = devInfo->getName();
    serialNumber_ = devInfo->getSerialNumber();
}

TimestampCollector::~TimestampCollector() {
    stop();
}

std::string TimestampCollector::getDeviceInfoStr() const {
    return deviceName_ + " (" + serialNumber_ + ")";
}

std::shared_ptr<ob::Device> TimestampCollector::getDevice() const {
    return device_;
}

std::vector<std::string> TimestampCollector::getOpenedFiles() const {
    return openedFiles_;
}

bool TimestampCollector::setupSensors(std::shared_ptr<ob::Config> &obConfig, const CmdLineConfig &toolConfig) {
    std::cout << "  Device: " << getDeviceInfoStr() << std::endl;

    auto isIntegralFps = [](double value) { return std::fabs(value - std::round(value)) < 1e-5; };

    auto printImuSensorEnabled = [](const std::string &sensorName, double sampleRate, const std::string &fullScaleRange) {
        std::cout << "    - " << sensorName << " sensor enabled";
        if(sampleRate > 0.0) {
            std::cout << " @" << sampleRate << "Hz";
        }
        if(!isWildcardToken(fullScaleRange)) {
            std::cout << " " << fullScaleRange;
        }
        std::cout << std::endl;
    };

    // Resolve which stream specs to use for this device
    const std::vector<StreamSpec> *specsPtr = nullptr;
    auto                           it       = toolConfig.deviceOverrides.find(serialNumber_);
    if(it != toolConfig.deviceOverrides.end()) {
        specsPtr = &it->second;
        std::cout << "  Using device-specific stream config (serial: " << serialNumber_ << ")" << std::endl;
    }
    else if(!toolConfig.streams.empty()) {
        specsPtr = &toolConfig.streams;
        std::cout << "  Using global stream config" << std::endl;
    }

    if(specsPtr != nullptr) {
        // --- User-specified streams ---
        for(const auto &spec: *specsPtr) {
            if(isImuSensorName(spec.sensor)) {
                bool hasAccel = deviceHasSensor(device_, OB_SENSOR_ACCEL);
                bool hasGyro  = deviceHasSensor(device_, OB_SENSOR_GYRO);
                if(!hasAccel || !hasGyro) {
                    std::cerr << "  Warning: Sensor \"imu\" requires both accel and gyro on device, skipping." << std::endl;
                    continue;
                }

                OBIMUSampleRate sampleRate = ob::TypeHelper::convertOBIMUSampleRateValueToType(static_cast<float>(spec.fps));
                if(spec.fps > 0 && sampleRate == OB_SAMPLE_RATE_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported IMU sample rate \"" << spec.fps << "\", skipping." << std::endl;
                    continue;
                }

                OBAccelFullScaleRange accelRange = accelFullScaleRangeFromString(spec.accelFullScaleRange);
                if(!isWildcardToken(spec.accelFullScaleRange) && accelRange == OB_ACCEL_FS_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported accel full scale range \"" << spec.accelFullScaleRange << "\", skipping." << std::endl;
                    continue;
                }

                OBGyroFullScaleRange gyroRange = gyroFullScaleRangeFromString(spec.gyroFullScaleRange);
                if(!isWildcardToken(spec.gyroFullScaleRange) && gyroRange == OB_GYRO_FS_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported gyro full scale range \"" << spec.gyroFullScaleRange << "\", skipping." << std::endl;
                    continue;
                }

                const std::string accelName = ob::TypeHelper::convertOBSensorTypeToString(OB_SENSOR_ACCEL);
                const std::string gyroName  = ob::TypeHelper::convertOBSensorTypeToString(OB_SENSOR_GYRO);
                obConfig->enableAccelStream(accelRange, sampleRate);
                obConfig->enableGyroStream(gyroRange, sampleRate);
                enabledSensors_[OB_SENSOR_ACCEL] = accelName;
                enabledSensors_[OB_SENSOR_GYRO]  = gyroName;
                printImuSensorEnabled(accelName, spec.fps, spec.accelFullScaleRange);
                printImuSensorEnabled(gyroName, spec.fps, spec.gyroFullScaleRange);
                continue;
            }

            OBSensorType sensorType = sensorNameToType(spec.sensor);
            if(sensorType == OB_SENSOR_UNKNOWN) {
                std::cerr << "  Warning: Unknown sensor name \"" << spec.sensor << "\", skipping." << std::endl;
                continue;
            }
            if(!deviceHasSensor(device_, sensorType)) {
                std::cerr << "  Warning: Sensor \"" << spec.sensor << "\" not found on device, skipping." << std::endl;
                continue;
            }

            OBFormat format = formatStringToOBFormat(spec.format);

            std::string sensorName = ob::TypeHelper::convertOBSensorTypeToString(sensorType);
            if(ob_is_video_sensor_type(sensorType)) {
                if(spec.fps > 0.0 && !isIntegralFps(spec.fps)) {
                    std::cerr << "  Warning: Video sensor \"" << spec.sensor << "\" requires integer fps, got \"" << spec.fps << "\", skipping." << std::endl;
                    continue;
                }

                obConfig->enableVideoStream(sensorType, static_cast<uint32_t>(spec.width), static_cast<uint32_t>(spec.height),
                                            static_cast<uint32_t>(std::llround(spec.fps)), format);
            }
            else if(sensorType == OB_SENSOR_ACCEL) {
                OBIMUSampleRate sampleRate = ob::TypeHelper::convertOBIMUSampleRateValueToType(static_cast<float>(spec.fps));
                if(spec.fps > 0 && sampleRate == OB_SAMPLE_RATE_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported accel sample rate \"" << spec.fps << "\", skipping." << std::endl;
                    continue;
                }

                OBAccelFullScaleRange accelRange = accelFullScaleRangeFromString(spec.accelFullScaleRange);
                if(!isWildcardToken(spec.accelFullScaleRange) && accelRange == OB_ACCEL_FS_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported accel full scale range \"" << spec.accelFullScaleRange << "\", skipping." << std::endl;
                    continue;
                }

                obConfig->enableAccelStream(accelRange, sampleRate);
            }
            else if(sensorType == OB_SENSOR_GYRO) {
                OBIMUSampleRate sampleRate = ob::TypeHelper::convertOBIMUSampleRateValueToType(static_cast<float>(spec.fps));
                if(spec.fps > 0 && sampleRate == OB_SAMPLE_RATE_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported gyro sample rate \"" << spec.fps << "\", skipping." << std::endl;
                    continue;
                }

                OBGyroFullScaleRange gyroRange = gyroFullScaleRangeFromString(spec.gyroFullScaleRange);
                if(!isWildcardToken(spec.gyroFullScaleRange) && gyroRange == OB_GYRO_FS_UNKNOWN) {
                    std::cerr << "  Warning: Unsupported gyro full scale range \"" << spec.gyroFullScaleRange << "\", skipping." << std::endl;
                    continue;
                }

                obConfig->enableGyroStream(gyroRange, sampleRate);
            }
            else {
                std::cerr << "  Warning: Unsupported sensor type \"" << spec.sensor << "\" for custom stream config, skipping." << std::endl;
                continue;
            }

            enabledSensors_[sensorType] = sensorName;

            std::cout << "    - " << sensorName << " sensor enabled";
            if(ob_is_video_sensor_type(sensorType) && spec.width > 0 && spec.height > 0) {
                std::cout << " (" << spec.width << "x" << spec.height << ")";
            }
            if(spec.fps > 0.0) {
                std::cout << " @" << spec.fps << (ob_is_video_sensor_type(sensorType) ? "fps" : "Hz");
            }
            if(sensorType == OB_SENSOR_ACCEL && !isWildcardToken(spec.accelFullScaleRange)) {
                std::cout << " " << spec.accelFullScaleRange;
            }
            if(sensorType == OB_SENSOR_GYRO && !isWildcardToken(spec.gyroFullScaleRange)) {
                std::cout << " " << spec.gyroFullScaleRange;
            }
            if(ob_is_video_sensor_type(sensorType) && !spec.format.empty()) {
                std::cout << " " << spec.format;
            }
            std::cout << std::endl;
        }
    }
    else {
        // --- Auto-select: prefer depth+color; fallback to color_left+color_right ---
        std::cout << "  No stream config specified, using auto-select" << std::endl;

        bool hasDepth      = deviceHasSensor(device_, OB_SENSOR_DEPTH);
        bool hasColor      = deviceHasSensor(device_, OB_SENSOR_COLOR);
        bool hasColorLeft  = deviceHasSensor(device_, OB_SENSOR_COLOR_LEFT);
        bool hasColorRight = deviceHasSensor(device_, OB_SENSOR_COLOR_RIGHT);

        std::vector<OBSensorType> autoTypes;
        if(hasDepth && hasColor) {
            autoTypes = { OB_SENSOR_DEPTH, OB_SENSOR_COLOR };
        }
        else if(hasColorLeft && hasColorRight) {
            autoTypes = { OB_SENSOR_COLOR_LEFT, OB_SENSOR_COLOR_RIGHT };
        }

        for(auto sensorType: autoTypes) {
            auto sensorName             = ob::TypeHelper::convertOBSensorTypeToString(sensorType);
            enabledSensors_[sensorType] = sensorName;
            obConfig->enableStream(sensorType);
            std::cout << "    - " << sensorName << " sensor enabled" << std::endl;
        }
    }

    if(enabledSensors_.empty()) {
        std::cerr << "Error: No sensors could be enabled on device " << getDeviceInfoStr() << std::endl;
        return false;
    }

    return true;
}

bool TimestampCollector::start(const CmdLineConfig &config) {
    if(running_) {
        return true;
    }

    std::cout << "Starting collector for " << getDeviceInfoStr() << std::endl;

    if(config.syncIntervalSec == 0) {
        std::cout << "  Performing one-time time sync..." << std::endl;
        device_->timerSyncWithHost();
    }

    // Disable auto exposure and set fixed exposure time for timestamp measurement stability
    std::cout << "  Disabling auto exposure..." << std::endl;

    // Depth sensor exposure settings
    if(device_->isPropertySupported(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PERMISSION_WRITE)) {
        device_->setBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, false);
    }
    if(device_->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
        device_->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, 3000);  // unit: us
    }

    // Color sensor exposure settings
    if(device_->isPropertySupported(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_WRITE)) {
        device_->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, false);
    }
    if(device_->isPropertySupported(OB_PROP_COLOR_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
        device_->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, 30);  // unit: 100us
    }

    // enable GlobalTimestamp if support
    if(device_->isGlobalTimestampSupported()) {
        try {
            device_->enableGlobalTimestamp(true);
        }
        catch(...) {
            // ignore error
            std::cerr << "Enable global timestamp failed, ignore" << std::endl;
        }
    }

    auto obConfig = std::make_shared<ob::Config>();
    obConfig->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_DISABLE);
    if(!setupSensors(obConfig, config)) {
        return false;
    }

    for(const auto &kv: enabledSensors_) {
        auto               sensorType = kv.first;
        const std::string &sensorName = kv.second;
        writers_[sensorType]          = std::make_shared<CsvWriter>();
        if(!writers_[sensorType]->open(serialNumber_, sensorName)) {
            std::cerr << "Failed to create CSV writer for " << sensorName << std::endl;
            return false;
        }
    }

    pipeline_      = std::make_shared<ob::Pipeline>(device_);
    running_       = true;
    processThread_ = std::thread(&TimestampCollector::processFrames, this);

    pipeline_->start(obConfig, [this](std::shared_ptr<ob::FrameSet> frameSet) {
        // Capture system timestamp immediately - before acquiring the lock - for maximum accuracy.
        uint64_t recvTimeUs = clockType_ == OB_CLOCK_TYPE_REALTIME ? getWallTimesUs() : getSteadyTimeUs();
        {
            std::lock_guard<std::mutex> lock(frameQueueMutex_);
            if(!running_) {
                return;
            }
            frameQueue_.push({ frameSet, recvTimeUs });
        }
        frameQueueCv_.notify_one();
    });

    std::cout << "  Collector started successfully" << std::endl;
    return true;
}

void TimestampCollector::stop() {
    if(!running_.exchange(false)) {
        return;
    }

    std::cout << "Stopping collector for " << getDeviceInfoStr() << std::endl;

    frameQueueCv_.notify_all();

    if(pipeline_) {
        try {
            pipeline_->stop();
        }
        catch(ob::Error &e) {
            std::cerr << "  Stop pipeline error: " << e.what() << std::endl;
        }
        pipeline_.reset();
    }

    if(processThread_.joinable()) {
        processThread_.join();
    }

    for(const auto &kv: writers_) {
        const auto &files = kv.second->getOpenedFiles();
        openedFiles_.insert(openedFiles_.end(), files.begin(), files.end());
    }
    writers_.clear();

    std::cout << "  Collector stopped" << std::endl;
}

bool TimestampCollector::isRunning() const {
    return running_;
}

void TimestampCollector::processFrames() {
    while(running_) {
        std::queue<TimedFrameSet> localQueue;
        {
            std::unique_lock<std::mutex> lock(frameQueueMutex_);
            frameQueueCv_.wait_for(lock, std::chrono::milliseconds(100), [this] { return !running_ || !frameQueue_.empty(); });

            if(!running_) {
                break;
            }
            if(frameQueue_.empty()) {
                continue;
            }
            frameQueue_.swap(localQueue);
        }

        while(!localQueue.empty()) {
            auto item = localQueue.front();
            localQueue.pop();
            if(item.frameSet) {
                onFrameSet(item.frameSet, item.recvTimeUs);
            }
        }
    }

    std::lock_guard<std::mutex> lock(frameQueueMutex_);
    while(!frameQueue_.empty()) {
        auto item = frameQueue_.front();
        frameQueue_.pop();
        if(item.frameSet) {
            onFrameSet(item.frameSet, item.recvTimeUs);
        }
    }
}

void TimestampCollector::onFrameSet(std::shared_ptr<ob::FrameSet> frameSet, uint64_t recvTimeUs) {
    uint32_t count = frameSet->getCount();
    for(uint32_t i = 0; i < count; ++i) {
        auto frame = frameSet->getFrameByIndex(i);
        if(!frame) {
            continue;
        }
        auto sensorType = ob_frame_type_to_sensor_type(frame->getType());
        if(writers_.count(sensorType)) {
            writers_[sensorType]->writeFrame(frame, recvTimeUs);
        }
    }
}

}  // namespace tools
}  // namespace libobsensor
