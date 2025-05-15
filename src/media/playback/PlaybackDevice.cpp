// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDevice.hpp"
#include "PlaybackDeviceParamManager.hpp"
#include "PlaybackFilterCreationStrategy.hpp"
#include "PlaybackDepthWorkModeManager.hpp"
#include "exception/ObException.hpp"
#include "DevicePids.hpp"
#include "component/frameprocessor/FrameProcessor.hpp"
#include "component/sensor/video/DisparityBasedSensor.hpp"
#include "component/metadata/FrameMetadataParserContainer.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"
#include "component/sensor/imu/GyroSensor.hpp"
#include "component/sensor/imu/AccelSensor.hpp"
#include "gemini330/G330FrameMetadataParserContainer.hpp"
#include "FilterFactory.hpp"

namespace libobsensor {
using namespace playback;

PlaybackDevice::PlaybackDevice(const std::string &filePath) : filePath_(filePath), port_(std::make_shared<PlaybackDevicePort>(filePath)) {
    init();
}

PlaybackDevice::~PlaybackDevice() {}

void PlaybackDevice::init() {
    fetchDeviceInfo();
    fetchExtensionInfo();

    initSensorList();
    initProperties();

    if(isDeviceInSeries(G330DevPids, deviceInfo_->pid_)) {
        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
#ifdef __linux__
            if(port_->getDeviceInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2) {
                container = std::make_shared<G330ColorFrameMetadataParserContainerByScr>(this, G330DeviceTimeFreq_, G330FrameTimeFreq_);
                return container;
            }
#endif
            container = std::make_shared<G330ColorFrameMetadataParserContainer>(this);
            return container;
        });

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
#ifdef __linux__
            if(port_->getDeviceInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2) {
                container = std::make_shared<G330DepthFrameMetadataParserContainerByScr>(this, G330DeviceTimeFreq_, G330FrameTimeFreq_);
                return container;
            }
#endif
            container = std::make_shared<G330DepthFrameMetadataParserContainer>(this);
            return container;
        });
    }

    auto algParamManager = std::make_shared<PlaybackDeviceParamManager>(this, port_);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    // Note: LiDAR not supported this component
    auto depthWorkModeManager = std::make_shared<PlaybackDepthWorkModeManager>(this, port_);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);
}

void PlaybackDevice::fetchDeviceInfo() {
    sensorTypeList_ = port_->getSensorList();
    deviceInfo_     = port_->getDeviceInfo();
}

void PlaybackDevice::fetchExtensionInfo() {
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void PlaybackDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    registerComponent(
        OB_DEV_COMPONENT_DEPTH_SENSOR,
        [this]() {
            std::shared_ptr<VideoSensor> sensor;
            if(isDeviceInSeries(FemtoMegaDevPids, deviceInfo_->pid_) || isDeviceInSeries(FemtoBoltDevPids, deviceInfo_->pid_)) {
                sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
            }
            else {
                sensor = std::make_shared<DisparityBasedSensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
                sensor->updateFormatFilterConfig({});  // for call convertProfileAsDisparityBasedProfile

                auto algParamManager = getComponentT<PlaybackDeviceParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
                algParamManager->bindDisparityParam(sensor->getStreamProfileList());

                auto propServer = getPropertyServer();

                auto hwD2D = propServer->getPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL);
                std::dynamic_pointer_cast<DisparityBasedSensor>(sensor)->markOutputDisparityFrame(!hwD2D);
            }

            if(isDeviceInSeries(G330DevPids, deviceInfo_->pid_)) {
                auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());
            }

            auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
            if(frameProcessor) {
                sensor->setFrameProcessor(frameProcessor.get());
            }

            return sensor;
        },
        true);

    registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);

        if(frameProcessor) {
            LOG_DEBUG("Try to init playback depth frame processor property");
            auto                              server          = getPropertyServer();
            const std::array<OBPropertyID, 4> depthProperties = { OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL,
                                                                  OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT,
                                                                  OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT };
            for(auto property: depthProperties) {
                OBPropertyValue value{};
                if(port_->getRecordedPropertyValue(property, &value)) {
                    frameProcessor->setPropertyValue(property, value);
                }
            }
        }
        else {
            LOG_WARN("Failed to create playback depth frame processor");
        }

        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_COLOR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR));

        if(isDeviceInSeries(G330DevPids, deviceInfo_->pid_)) {
            auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER);
            sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());
        }

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_IR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_IR));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_LEFT_IR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_LEFT, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_IR_LEFT));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        if(isDeviceInSeries(G330DevPids, deviceInfo_->pid_)) {
            auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
            sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());
        }

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_LEFT);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_RIGHT_IR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_RIGHT, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_IR_RIGHT));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        if(isDeviceInSeries(G330DevPids, deviceInfo_->pid_)) {
            auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
            sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());
        }

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_RIGHT);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_GYRO_SENSOR, [this]() {
        auto sensor = std::make_shared<GyroSensor>(this, port_, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_GYRO));

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_ACCEL_SENSOR, [this]() {
        auto sensor = std::make_shared<AccelSensor>(this, port_, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_ACCEL));

        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this]() {
        auto propertyAccessor = std::make_shared<PlaybackVendorPropertyAccessor>(port_, this);

        return propertyAccessor;
    });
}

void PlaybackDevice::initProperties() {
    frameTransformAccessor_ = std::make_shared<PlaybackFrameTransformPropertyAccessor>(port_, this);
    auto propertyServer     = std::make_shared<PropertyServer>(this);
    auto filterAccessor     = std::make_shared<PlaybackFilterPropertyAccessor>(port_, this);
    auto vendorAccessor     = getComponentT<PlaybackVendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR).get();

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    // common imu properties
    registerPropertyCondition(propertyServer, OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "r", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "r", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "r", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "r", vendorAccessor, nullptr, true);
    // These properties are set as writable only to ensure compatibility with the IMU sensor startup.
    registerPropertyCondition(propertyServer, OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_PROP_ACCEL_SWITCH_BOOL, "rw", "rw", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorAccessor, nullptr, true);
    registerPropertyCondition(propertyServer, OB_PROP_GYRO_SWITCH_BOOL, "rw", "rw", vendorAccessor, nullptr, true);

    // filter properties
    registerPropertyCondition(propertyServer, OB_PROP_DISPARITY_TO_DEPTH_BOOL, "r", "r", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, "rw", "rw", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, "rw", "rw", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, "rw", "rw", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT, "r", "r", vendorAccessor);

    // Exposure properties
    // Depth sensor properties
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_GAIN_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "r", "r", vendorAccessor);
    // Color sensor properties
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_GAIN_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_WHITE_BALANCE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_BRIGHTNESS_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_SHARPNESS_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_SATURATION_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_CONTRAST_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_GAMMA_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_HUE_INT, "r", "r", vendorAccessor);
    // IR sensor properties
    registerPropertyCondition(propertyServer, OB_PROP_IR_AUTO_EXPOSURE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_IR_AE_MAX_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_IR_BRIGHTNESS_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_IR_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_IR_GAIN_INT, "r", "r", vendorAccessor);

    // G330 metadata properties(v4l2 backend only)
    // Color sensor properties
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_STRUCT_COLOR_AE_ROI, "r", "r", vendorAccessor);
    // Depth sensor properties
    registerPropertyCondition(propertyServer, OB_STRUCT_DEPTH_HDR_CONFIG, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_STRUCT_DEPTH_AE_ROI, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_STRUCT_DEVICE_TIME, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DISP_SEARCH_RANGE_MODE_INT, "r", "r", vendorAccessor);

    // Mirror, Flip, Rotation properties
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_ROTATE_INT, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_ROTATE_INT, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_ROTATE_INT, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_RIGHT_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_RIGHT_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_IR_RIGHT_ROTATE_INT, "rw", "rw", frameTransformAccessor_);
}

std::vector<std::shared_ptr<IFilter>> PlaybackDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    auto filterStrategyFactory = FilterCreationStrategyFactory::getInstance();  // namespace playback
    auto filterStrategy        = filterStrategyFactory->create(deviceInfo_->pid_);
    if(filterStrategy) {
        return filterStrategy->createFilters(type);
    }
    return {};
}

void PlaybackDevice::registerPropertyCondition(std::shared_ptr<PropertyServer> server, uint32_t propertyId, const std::string &userPermsStr,
                                               const std::string &intPermsStr, std::shared_ptr<IPropertyAccessor> accessor, ConditionCheckHandler condition,
                                               bool skipSupportCheck) {
    if(condition && !condition()) {
        return;
    }

    if(!skipSupportCheck && !port_->isPropertySupported(propertyId)) {
        return;
    }

    server->registerProperty(propertyId, userPermsStr, intPermsStr, accessor);
}

void PlaybackDevice::pause() {
    port_->pause();
}

void PlaybackDevice::resume() {
    port_->resume();
}

void PlaybackDevice::seek(const uint64_t timestamp) {
    port_->seek(timestamp);
}

void PlaybackDevice::setPlaybackRate(const float rate) {
    port_->setPlaybackRate(rate);
}

uint64_t PlaybackDevice::getDuration() const {
    return port_->getDuration();
}

uint64_t PlaybackDevice::getPosition() const {
    return port_->getPosition();
}

std::vector<OBSensorType> PlaybackDevice::getSensorTypeList() const {
    return sensorTypeList_;
}

bool PlaybackDevice::isDeviceInSeries(const std::vector<uint16_t> &pids, const uint16_t &pid) {
    return std::find(pids.begin(), pids.end(), pid) != pids.end() ? true : false;
}

void PlaybackDevice::setPlaybackStatusCallback(const PlaybackStatusCallback callback) {
    port_->setPlaybackStatusCallback(callback);
}

OBPlaybackStatus PlaybackDevice::getCurrentPlaybackStatus() const {
    return port_->getCurrentPlaybackStatus();
}

void PlaybackDevice::fetchProperties() {
    frameTransformAccessor_->initFrameTransformProperty();
}

}  // namespace libobsensor