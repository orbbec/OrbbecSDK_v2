// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDevice.hpp"
#include "PlaybackDeviceParamManager.hpp"
#include "PlaybackFilterCreationStrategy.hpp"
#include "PlaybackDepthWorkModeManager.hpp"
#include "PlaybackPresetManager.hpp"
#include "PlaybackDeviceSyncConfigurator.hpp"
#include "exception/ObException.hpp"
#include "DevicePids.hpp"
#include "component/frameprocessor/FrameProcessor.hpp"
#include "component/sensor/video/DisparityBasedSensor.hpp"
#include "component/metadata/FrameMetadataParserContainer.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"
#include "component/sensor/imu/GyroSensor.hpp"
#include "component/sensor/imu/AccelSensor.hpp"
#include "component/sensor/lidar/LiDARStreamer.hpp"
#include "component/sensor/lidar/LiDARSensor.hpp"
#include "component/sensor/lidar/LiDARGyroSensor.hpp"
#include "component/sensor/lidar/LiDARAccelSensor.hpp"
#include "gemini330/G330FrameMetadataParserContainer.hpp"
#include "gemini2/G435LeFrameMetadataParserContainer.hpp"
#include "openni/OpenNIDisparitySensor.hpp"
#include "openni/DW2DisparitySensor.hpp"
#include "openni/MaxDisparitySensor.hpp"
#include "FilterFactory.hpp"
#include "PlaybackFrameInterleaveManager.hpp"
#include "gemini330/G330FrameInterleaveManager.hpp"
#include "gemini2/G435LeFrameInterleaveManager.hpp"
#include "gemini305/G305FrameMetadataParserContainer.hpp"

namespace libobsensor {
using namespace playback;

PlaybackDevice::PlaybackDevice(const std::string &filePath) : filePath_(filePath), port_(std::make_shared<PlaybackDevicePort>(filePath)) {
    isPlaybackDevice_ = true;
    init();
}

PlaybackDevice::~PlaybackDevice() {}

void PlaybackDevice::init() {
    fetchDeviceInfo();
    fetchExtensionInfo();

    initSensorList();
    initProperties();

    auto vid = deviceInfo_->vid_;
    auto pid = deviceInfo_->pid_;

    if(isDeviceInContainer(G330DevPids, vid, pid)) {
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
    else if(isDeviceInContainer(G435LeDevPids, vid, pid)) {
        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
            container = std::make_shared<G435LeColorFrameMetadataParserContainer>(this);
            return container;
        });

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
            container = std::make_shared<G435LeDepthFrameMetadataParserContainer>(this);
            return container;
        });
    }
    else if(isDeviceInOrbbecSeries(G305DevPids, vid, pid)) {
        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
#ifdef __linux__
            if(port_->getDeviceInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2) {
                container = std::make_shared<G305ColorFrameMetadataParserContainerByScr>(this, G330DeviceTimeFreq_, G330FrameTimeFreq_);
                return container;
            }
#endif
            container = std::make_shared<G305ColorFrameMetadataParserContainer>(this);
            return container;
        });

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, [this]() {
            std::shared_ptr<FrameMetadataParserContainer> container;
#ifdef __linux__
            if(port_->getDeviceInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2) {
                container = std::make_shared<G305DepthFrameMetadataParserContainerByScr>(this, G330DeviceTimeFreq_, G330FrameTimeFreq_);
                return container;
            }
#endif
            container = std::make_shared<G305DepthFrameMetadataParserContainer>(this);
            return container;
        });
    }

    if(!isDeviceInOrbbecSeries(LiDARDevPids, vid, pid)) {
        auto algParamManager = std::make_shared<PlaybackDeviceParamManager>(this, port_);
        registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

        // Note: LiDAR not supported this component
        auto depthWorkModeManager = std::make_shared<PlaybackDepthWorkModeManager>(this, port_);
        registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);
    }

    if(isDeviceInContainer(G330DevPids, vid, pid) || isDeviceInOrbbecSeries(FemtoMegaDevPids, vid, pid) || isDeviceInOrbbecSeries(FemtoBoltDevPids, vid, pid)
       || isDeviceInOrbbecSeries(G305DevPids, vid, pid)) {
        // preset manager
        auto presetManager = std::make_shared<PlaybackPresetManager>(this);
        registerComponent(OB_DEV_COMPONENT_PRESET_MANAGER, presetManager);
    }

    if(port_->isPropertySupported(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL)) {
        OBPropertyValue interleaveEnable{};
        port_->getRecordedPropertyValue(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, &interleaveEnable);

        if(interleaveEnable.intValue > 0) {
            std::shared_ptr<IFrameInterleaveManager> devFrameInterleaveManager;
            if(isDeviceInContainer(G330DevPids, vid, pid)) {
                devFrameInterleaveManager = std::make_shared<G330FrameInterleaveManager>(this);
            }
            else {
                devFrameInterleaveManager = std::make_shared<G435LeFrameInterleaveManager>(this);
            }

            auto frameInterleaveManagerProxy = std::make_shared<libobsensor::PlaybackFrameInterleaveManager>(devFrameInterleaveManager);
            registerComponent(OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER, frameInterleaveManagerProxy);
        }
    }
}

void PlaybackDevice::fetchDeviceInfo() {
    sensorTypeList_ = port_->getSensorList();
    deviceInfo_     = port_->getDeviceInfo();
}

void PlaybackDevice::fetchExtensionInfo() {
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void PlaybackDevice::initSensorList() {
    auto vid = deviceInfo_->vid_;
    auto pid = deviceInfo_->pid_;

    if(isDeviceInOrbbecSeries(LiDARDevPids, vid, pid)) {
        // LiDAR: only register lidar sensor
        registerComponent(
            OB_DEV_COMPONENT_LIDAR_SENSOR,
            [this]() {
                auto sensor = std::make_shared<LiDARSensor>(this, port_, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_LIDAR));

                // disable timestamp anomaly detection for playback device
                sensor->enableTimestampAnomalyDetection(false);
                return sensor;
            },
            true);

        registerComponent(OB_DEV_COMPONENT_GYRO_SENSOR, [this]() {
            auto sensor = std::make_shared<LiDARGyroSensor>(this, port_, port_);
            sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_GYRO));

            // disable timestamp anomaly detection for playback device
            sensor->enableTimestampAnomalyDetection(false);
            return sensor;
        });

        registerComponent(OB_DEV_COMPONENT_ACCEL_SENSOR, [this]() {
            auto sensor = std::make_shared<LiDARAccelSensor>(this, port_, port_);
            sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_ACCEL));

            // disable timestamp anomaly detection for playback device
            sensor->enableTimestampAnomalyDetection(false);
            return sensor;
        });
        return;
    }

    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    registerComponent(
        OB_DEV_COMPONENT_DEPTH_SENSOR,
        [this]() {
            std::shared_ptr<VideoSensor> sensor;
            auto                         vid = deviceInfo_->vid_;
            auto                         pid = deviceInfo_->pid_;
            if(isDeviceInOrbbecSeries(FemtoMegaDevPids, vid, pid) || isDeviceInOrbbecSeries(FemtoBoltDevPids, vid, pid)) {
                sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
            }
            else if(isDeviceInOrbbecSeries(OpenniMonocularPids, vid, pid)) {
                sensor = std::make_shared<OpenNIDisparitySensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
                sensor->updateFormatFilterConfig({});  // for call convertProfileAsDisparityBasedProfile

                auto algParamManager = getComponentT<PlaybackDeviceParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
                algParamManager->bindDisparityParam(sensor->getStreamProfileList());

                auto propServer = getPropertyServer();
                std::dynamic_pointer_cast<OpenNIDisparitySensor>(sensor)->markOutputDisparityFrame(true);

                // depth unit - non-G330 specific
                if(port_->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT)) {
                    OBPropertyValue value{};
                    port_->getRecordedPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, &value);
                    std::dynamic_pointer_cast<OpenNIDisparitySensor>(sensor)->setDepthUnit(
                        utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue)));
                    propServer->setPropertyValueT<int>(OB_PROP_DEPTH_PRECISION_LEVEL_INT, value.intValue);
                }
            }
            else if(isDeviceInOrbbecSeries(OpenniDW2Pids, vid, pid)) {
                sensor = std::make_shared<DW2DisparitySensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
                sensor->updateFormatFilterConfig({});  // for call convertProfileAsDisparityBasedProfile

                auto algParamManager = getComponentT<PlaybackDeviceParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
                algParamManager->bindDisparityParam(sensor->getStreamProfileList());

                auto processParam = algParamManager->getOpenNIFrameProcessParam();
                std::dynamic_pointer_cast<DW2DisparitySensor>(sensor)->setFrameProcessParam(processParam);

                auto propServer = getPropertyServer();
                std::dynamic_pointer_cast<DW2DisparitySensor>(sensor)->markOutputDisparityFrame(true);

                // depth unit - non-G330 specific
                if(port_->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT)) {
                    OBPropertyValue value{};
                    port_->getRecordedPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, &value);
                    std::dynamic_pointer_cast<DW2DisparitySensor>(sensor)->setDepthUnit(
                        utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue)));
                    propServer->setPropertyValueT<int>(OB_PROP_DEPTH_PRECISION_LEVEL_INT, value.intValue);
                }
            }
            else if(isDeviceInOrbbecSeries(OpenniMaxPids, vid, pid)) {
                sensor = std::make_shared<MaxDisparitySensor>(this, OB_SENSOR_DEPTH, port_);
                sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_DEPTH));
                sensor->updateFormatFilterConfig({});  // for call convertProfileAsDisparityBasedProfile

                auto algParamManager = getComponentT<PlaybackDeviceParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
                algParamManager->bindDisparityParam(sensor->getStreamProfileList());

                auto processParam = algParamManager->getOpenNIFrameProcessParam();
                std::dynamic_pointer_cast<MaxDisparitySensor>(sensor)->setFrameProcessParam(processParam);

                auto propServer = getPropertyServer();
                std::dynamic_pointer_cast<MaxDisparitySensor>(sensor)->markOutputDisparityFrame(true);

                // depth unit - non-G330 specific
                if(port_->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT)) {
                    OBPropertyValue value{};
                    port_->getRecordedPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, &value);
                    std::dynamic_pointer_cast<MaxDisparitySensor>(sensor)->setDepthUnit(
                        utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue)));
                    propServer->setPropertyValueT<int>(OB_PROP_DEPTH_PRECISION_LEVEL_INT, value.intValue);
                }
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

                // init depth unit property
                if(isDeviceInContainer(G330DevPids, vid, pid)) {
                    // G330 specific
                    if(port_->isPropertySupported(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT)) {
                        OBPropertyValue value{};

                        port_->getRecordedPropertyValue(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT, &value);
                        std::dynamic_pointer_cast<DisparityBasedSensor>(sensor)->setDepthUnit(value.floatValue);
                        propServer->setPropertyValueT<float>(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT, value.floatValue);
                    }
                }
                else {
                    // non-G330 specific
                    if(port_->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT)) {
                        OBPropertyValue value{};
                        port_->getRecordedPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, &value);
                        std::dynamic_pointer_cast<DisparityBasedSensor>(sensor)->setDepthUnit(
                            utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue)));
                        propServer->setPropertyValueT<int>(OB_PROP_DEPTH_PRECISION_LEVEL_INT, value.intValue);
                    }
                }
            }

            // register metadata parser container if the device have metadata
            auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, false);
            if(depthMdParserContainer) {
                sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());
            }

            auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
            if(frameProcessor) {
                sensor->setFrameProcessor(frameProcessor.get());
            }

            // disable timestamp anomaly detection for playback device
            sensor->enableTimestampAnomalyDetection(false);
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

        auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER, false);
        if(colorMdParserContainer) {
            sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_LEFT_COLOR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_LEFT, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR_LEFT));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR_LEFT));

        auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_METADATA_CONTAINER, false);
        if(colorMdParserContainer) {
            sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_LEFT);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_RIGHT_COLOR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR_RIGHT, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR_RIGHT));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_COLOR_RIGHT));

        auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_METADATA_CONTAINER, false);
        if(colorMdParserContainer) {
            sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, [this]() {
        auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
        auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR_RIGHT);
        return frameProcessor;
    });

    registerComponent(OB_DEV_COMPONENT_IR_SENSOR, [this]() {
        auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_IR));

        auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, false);
        if(frameProcessor) {
            sensor->setFrameProcessor(frameProcessor.get());
        }

        // Currently IR sensor does not supports this component, implemented here for possible compatibility
        auto metaParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, false);
        if(metaParserContainer) {
            sensor->setFrameMetadataParserContainer(metaParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
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

        auto metaParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, false);
        if(metaParserContainer) {
            sensor->setFrameMetadataParserContainer(metaParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
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

        auto metaParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, false);
        if(metaParserContainer) {
            sensor->setFrameMetadataParserContainer(metaParserContainer.get());
        }

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
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

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
        return sensor;
    });

    registerComponent(OB_DEV_COMPONENT_ACCEL_SENSOR, [this]() {
        auto sensor = std::make_shared<AccelSensor>(this, port_, port_);
        sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_ACCEL));

        // disable timestamp anomaly detection for playback device
        sensor->enableTimestampAnomalyDetection(false);
        return sensor;
    });

    if(isDeviceInContainer(G435LeDevPids, vid, pid)) {
        registerComponent(OB_DEV_COMPONENT_CONFIDENCE_SENSOR, [this]() {
            auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_CONFIDENCE, port_);
            sensor->setStreamProfileList(port_->getStreamProfileList(OB_SENSOR_CONFIDENCE));

            auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_CONFIDENCE_FRAME_PROCESSOR, false);
            if(frameProcessor) {
                sensor->setFrameProcessor(frameProcessor.get());
            }

            auto metaParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER, false);
            if(metaParserContainer) {
                sensor->setFrameMetadataParserContainer(metaParserContainer.get());
            }

            // disable timestamp anomaly detection for playback device
            sensor->enableTimestampAnomalyDetection(false);
            return sensor;
        });

        registerComponent(OB_DEV_COMPONENT_CONFIDENCE_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_CONFIDENCE);
            return frameProcessor;
        });
    }

    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this]() {
        auto propertyAccessor = std::make_shared<PlaybackVendorPropertyAccessor>(port_, this);

        return propertyAccessor;
    });
}

void PlaybackDevice::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    if(isDeviceInOrbbecSeries(LiDARDevPids, deviceInfo_->vid_, deviceInfo_->pid_)) {
        // LiDAR: no any property for playback device
        return;
    }

    frameTransformAccessor_ = std::make_shared<PlaybackFrameTransformPropertyAccessor>(port_, this);
    auto filterAccessor     = std::make_shared<PlaybackFilterPropertyAccessor>(port_, this);
    auto vendorAccessor     = getComponentT<PlaybackVendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR).get();

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
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT, "r", "rw", filterAccessor);  // G330 specific
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_PRECISION_LEVEL_INT, "r", "rw", filterAccessor);             // Other devices
    registerPropertyCondition(propertyServer, OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST, "r", "r", filterAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT, "r", "r", vendorAccessor);

    // laser
    registerPropertyCondition(propertyServer, OB_PROP_LASER_CONTROL_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, "r", "r", vendorAccessor);

    // Exposure properties
    // Depth sensor properties
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_EXPOSURE_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_GAIN_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "r", "rw", vendorAccessor);
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
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, "r", "r", vendorAccessor);
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

    // Interleave config property
    registerPropertyCondition(propertyServer, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, "r", "r", vendorAccessor);
    registerPropertyCondition(propertyServer, OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, "r", "r", vendorAccessor);

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
    registerPropertyCondition(propertyServer, OB_PROP_CONFIDENCE_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_CONFIDENCE_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_CONFIDENCE_ROTATE_INT, "rw", "rw", frameTransformAccessor_);

    registerPropertyCondition(propertyServer, OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "r", "r", vendorAccessor);

    // G305 metadata properties
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_RIGHT_MIRROR_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_RIGHT_FLIP_BOOL, "rw", "rw", frameTransformAccessor_);
    registerPropertyCondition(propertyServer, OB_PROP_COLOR_RIGHT_ROTATE_INT, "rw", "rw", frameTransformAccessor_);

    // Note: Multi-device sync config: made up of multiple parts
    // 1. Exposed through ob_device_get_multi_device_sync_config, but restricted to internal use
    // 2. If the property exists, register it for read access
    if(port_->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG)) {
        registerPropertyCondition(propertyServer, OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "", "r", vendorAccessor);

        auto deviceSyncConfigurator = std::make_shared<PlaybackDeviceSyncConfigurator>(this);
        registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);
    }
}

std::vector<std::shared_ptr<IFilter>> PlaybackDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    auto filterStrategyFactory = FilterCreationStrategyFactory::getInstance();  // namespace playback
    auto filterStrategy        = filterStrategyFactory->create(deviceInfo_->vid_, deviceInfo_->pid_);
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

void PlaybackDevice::setPlaybackStatusCallback(const PlaybackStatusCallback callback) {
    port_->setPlaybackStatusCallback(callback);
}

OBPlaybackStatus PlaybackDevice::getCurrentPlaybackStatus() const {
    return port_->getCurrentPlaybackStatus();
}

void PlaybackDevice::fetchProperties() {
    if(frameTransformAccessor_) {
        frameTransformAccessor_->initFrameTransformProperty();
    }
}

}  // namespace libobsensor