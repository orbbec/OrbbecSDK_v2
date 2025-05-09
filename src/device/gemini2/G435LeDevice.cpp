// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G435LeDevice.hpp"

#include "DevicePids.hpp"
#include "InternalTypes.hpp"

#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "stream/StreamProfileFactory.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "property/PrivateFilterPropertyAccessors.hpp"
#include "monitor/DeviceMonitor.hpp"
#include "syncconfig/DeviceSyncConfigurator.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "firmwareupdater/firmwareupdateguard/FirmwareUpdateGuards.hpp"

#include "G2AlgParamManager.hpp"
#include "G2StreamProfileFilter.hpp"
#include "G2PropertyAccessors.hpp"
#include "G2DepthWorkModeManager.hpp"
#include "G2FrameTimestampCalculator.hpp"
#include "G435LeFrameMetadataParserContainer.hpp"
#include "G435LeFrameInterleaveManager.hpp"

#include <algorithm>

#if defined(BUILD_NET_PAL)
#include "ethernet/RTSPStreamPort.hpp"
#endif

namespace libobsensor {

static const uint8_t INTERFACE_COLOR    = 0;
static const uint8_t INTERFACE_LEFT_IR  = 4;
static const uint8_t INTERFACE_DEPTH    = 2;
static const uint8_t INTERFACE_RIGHT_IR = 6;

G435LeDeviceBase::G435LeDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {}
G435LeDeviceBase::~G435LeDeviceBase() noexcept {}

void G435LeDeviceBase::init() {    
    fetchExtensionInfo();
    fetchDeviceInfo();

    auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
    registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

    auto algParamManager = std::make_shared<G435LeAlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    auto depthWorkModeManager = std::make_shared<G2DepthWorkModeManager>(this);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);

    static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes = {
        OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN,         OB_MULTI_DEVICE_SYNC_MODE_STANDALONE,          OB_MULTI_DEVICE_SYNC_MODE_PRIMARY,
        OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING
    };
    auto deviceSyncConfigurator = std::make_shared<DeviceSyncConfigurator>(this, supportedSyncModes);
    registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);

    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, [this] {  //
        return std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, deviceTimeFreq_);
    });

    auto propertyServer         = getPropertyServer();
    auto vendorPropertyAccessor = getComponentT<VendorPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    propertyServer->registerProperty(OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT, "rw", "rw", vendorPropertyAccessor.get());
    propertyServer->registerProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor.get());
    propertyServer->registerProperty(OB_PROP_FRAME_INTERLEAVE_LASER_PATTERN_SYNC_DELAY_INT, "rw", "rw", vendorPropertyAccessor.get());

    auto frameInterleaveManager = std::make_shared<G435LeFrameInterleaveManager>(this);
    registerComponent(OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER, frameInterleaveManager);

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

std::vector<std::shared_ptr<IFilter>> G435LeDeviceBase::createRecommendedPostProcessingFilters(OBSensorType type) {
    auto filterFactory = FilterFactory::getInstance();
    if(type == OB_SENSOR_DEPTH) {
        // activate depth frame processor library
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);

        std::vector<std::shared_ptr<IFilter>> depthFilterList;

        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            depthFilterList.push_back(decimationFilter);
        }

        if(filterFactory->isFilterCreatorExists("HDRMerge")) {
            auto hdrMergeFilter = filterFactory->createFilter("HDRMerge");
            depthFilterList.push_back(hdrMergeFilter);
        }

        if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
            auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
            depthFilterList.push_back(sequenceIdFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
            // magnitude, alpha, disp_diff, radius
            std::vector<std::string> params = { "1", "0.5", "64", "1" };
            spatFilter->updateConfig(params);
            depthFilterList.push_back(spatFilter);
        }

        if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
            auto tempFilter = filterFactory->createFilter("TemporalFilter");
            // diff_scale, weight
            std::vector<std::string> params = { "0.1", "0.4" };
            tempFilter->updateConfig(params);
            depthFilterList.push_back(tempFilter);
        }

        if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
            auto                     hfFilter = filterFactory->createFilter("HoleFillingFilter");
            std::vector<std::string> params   = { "2" };
            hfFilter->updateConfig(params);
            depthFilterList.push_back(hfFilter);
        }

        if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
            auto dtFilter = filterFactory->createFilter("DisparityTransform");
            depthFilterList.push_back(dtFilter);
        }

        if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
            auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
            depthFilterList.push_back(ThresholdFilter);
        }

        for(size_t i = 0; i < depthFilterList.size(); i++) {
            auto filter = depthFilterList[i];
            if(filter->getName() != "DisparityTransform") {
                filter->enable(false);
            }
        }
        return depthFilterList;
    }
    else if(type == OB_SENSOR_COLOR) {
        // activate color frame processor library
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);

        std::vector<std::shared_ptr<IFilter>> colorFilterList;
        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            decimationFilter->enable(false);
            colorFilterList.push_back(decimationFilter);
        }
        return colorFilterList;
    }
    else if(type == OB_SENSOR_IR_LEFT) {
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, false);
        std::vector<std::shared_ptr<IFilter>> leftIRFilterList;
        if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
            auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
            sequenceIdFilter->enable(false);
            leftIRFilterList.push_back(sequenceIdFilter);
            return leftIRFilterList;
        }
    }
    else if(type == OB_SENSOR_IR_RIGHT) {
        getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, false);
        std::vector<std::shared_ptr<IFilter>> rightIRFilterList;
        if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
            auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
            sequenceIdFilter->enable(false);
            rightIRFilterList.push_back(sequenceIdFilter);
            return rightIRFilterList;
        }
    }
    return {};
}

void G435LeDeviceBase::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto streamProfileFilter = getComponentT<IStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
    sensor->setStreamProfileFilter(streamProfileFilter.get());

    auto        depthWorkModeManager = getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto        workMode             = depthWorkModeManager->getCurrentDepthWorkMode();
    std::string workModeName         = workMode.name;
    auto        sensorType           = sensor->getSensorType();
    auto        streamProfile        = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType, workModeName);
    if(!streamProfile) {
        // if not found, try to get default stream profile without work mode
        streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    }
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }
}

G435LeDevice::G435LeDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : G435LeDeviceBase(info) {
    init();
}
G435LeDevice::~G435LeDevice() noexcept {}

void G435LeDevice::init() {
    initSensorList();
    initProperties();
    G435LeDeviceBase::init();

    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });

    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATE_GUARD_FACTORY, [this]() {
        std::shared_ptr<FirmwareUpdateGuardFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FirmwareUpdateGuardFactory>(this); })
        return factory;
    });
}

void G435LeDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    registerComponent(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER, [this]() { return std::make_shared<G2StreamProfileFilter>(this); });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_DEPTH;
    });
    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        auto port          = getSourcePort(depthPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto port   = getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<DisparityBasedSensor>(this, OB_SENSOR_DEPTH, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_MJPG, OB_FORMAT_RVL, nullptr },
                };
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());

                auto frameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                auto propServer = getPropertyServer();

                propServer->setPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL, true);
                propServer->setPropertyValueT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, false);
                sensor->markOutputDisparityFrame(false);
                
                auto depthPrecisionRange = propServer->getPropertyRangeT<int32_t>(OB_PROP_DEPTH_PRECISION_LEVEL_INT);  
                LOG_DEBUG("Depth precision level range: min={}, max={}, def={}, cur={}", depthPrecisionRange.min, depthPrecisionRange.max, depthPrecisionRange.def, depthPrecisionRange.cur);              
                propServer->setPropertyValueT(OB_PROP_DEPTH_PRECISION_LEVEL_INT, depthPrecisionRange.def);
                sensor->setDepthUnit( utils::depthPrecisionLevelToUnit((OBDepthPrecisionLevel)depthPrecisionRange.def));

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);

        registerSensorPortInfo(OB_SENSOR_DEPTH, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });
    }

    auto leftIrPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_IR_LEFT;
    });

    if(leftIrPortInfoIter != sourcePortInfoList.end()) {
        auto leftIrPortInfo = *leftIrPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_LEFT_IR_SENSOR,
            [this, leftIrPortInfo]() {
                auto port   = getSourcePort(leftIrPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_LEFT, port);

                auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());

                auto frameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR_LEFT, leftIrPortInfo);

        registerComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_LEFT);
            return frameProcessor;
        });
    }

    auto rightIrPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_IR_RIGHT;
    });
    if(rightIrPortInfoIter != sourcePortInfoList.end()) {
        auto rightIrPortInfo = *rightIrPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_RIGHT_IR_SENSOR,
            [this, rightIrPortInfo]() {
                auto port   = getSourcePort(rightIrPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_RIGHT, port);

                auto depthMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_DEPTH_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer.get());

                auto frameTimestampCalculator = std::make_shared<FrameTimestampCalculatorDirectly>(this, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR_RIGHT, rightIrPortInfo);
        registerComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_RIGHT);
            return frameProcessor;
        });
    }

    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_COLOR;
    });

    if(colorPortInfoIter != sourcePortInfoList.end()) {
        auto colorPortInfo = *colorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_COLOR_SENSOR,
            [this, colorPortInfo]() {
                auto port   = getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGRA, formatConverter });
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());

                auto frameTimestampCalculator = std::make_shared<G435LeFrameTimestampCalculatorDeviceTime>(this, deviceTimeFreq_, colorframeTimeFreq_, frameTimeFreq_);
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, colorframeTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_COLOR, colorPortInfo);

        registerComponent(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_COLOR);
            return frameProcessor;
        });
    }

    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_VENDOR_STREAM;
    });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter

            auto port               = getSourcePort(imuPortInfo);
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL, true);

            imuCorrectorFilter->enable(true);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto imuStreamer    = std::make_shared<ImuStreamer>(this, dataStreamPort, imuCorrectorFilter);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<AccelSensor>(this, port, imuStreamerSharedPtr);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_ACCEL, imuPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_GYRO_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<ImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<GyroSensor>(this, port, imuStreamerSharedPtr);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void G435LeDevice::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                                  [](const std::shared_ptr<const SourcePortInfo> &portInfo) {  //
                                               return portInfo->portType == SOURCE_PORT_NET_VENDOR;
                                           });
    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto vendorPortInfo         = *vendorPortInfoIter;
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return accessor;
    });

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, vendorPortInfo]() {
        auto port       = getSourcePort(vendorPortInfo);
        auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
        return devMonitor;
    });

    auto propertyServer = std::make_shared<PropertyServer>(this);
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_HUE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_IR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_IR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);  // using vendor property accessor
    propertyServer->registerProperty(OB_PROP_LDP_BOOL, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_LASER_BOOL, "rw", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_DEPTH_HOLEFILTER_BOOL, "rw", "rw", vendorPropertyAccessor);
    // propertyServer->registerProperty(OB_PROP_LDP_STATUS_BOOL, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LDP_MEASURE_DISTANCE_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->aliasProperty(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL);
    propertyServer->registerProperty(OB_PROP_TIMER_RESET_DELAY_US_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, "rw", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_DEPTH_ALG_MODE_LIST, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_DEPTH_CALIB_PARAM, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_SDK_DEPTH_FRAME_UNPACK_BOOL, "", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_IR_CHANNEL_DATA_SOURCE_INT, "rw", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_DEPTH_RM_FILTER_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_WATCHDOG_BOOL, "rw", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LASER_POWER_ACTUAL_LEVEL_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
    //propertyServer->registerProperty(OB_PROP_DEVICE_USB2_REPEAT_IDENTIFY_BOOL, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEVICE_RESET_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_IR_STREAM_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_IR_RIGHT_STREAM_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_STOP_COLOR_STREAM_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEBUG_ESGM_CONFIDENCE_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_COLOR_AE_ROI, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEPTH_AE_ROI, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_TEMPERATURE_COMPENSATION_BOOL, "rw", "rw", vendorPropertyAccessor);
    
    auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
    if(imuCorrectorFilter) {
        auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
        propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
        propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
    }
 
    propertyServer->aliasProperty(OB_PROP_LASER_CONTROL_INT, OB_PROP_LASER_BOOL);
    propertyServer->aliasProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PROP_IR_AUTO_EXPOSURE_BOOL);
    propertyServer->aliasProperty(OB_PROP_DEPTH_GAIN_INT, OB_PROP_IR_GAIN_INT);
    propertyServer->aliasProperty(OB_PROP_DEPTH_EXPOSURE_INT, OB_PROP_IR_EXPOSURE_INT);

    auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);

    auto baseLinePropertyAccessor = std::make_shared<BaselinePropertyAccessor>(this);
    propertyServer->registerProperty(OB_STRUCT_BASELINE_CALIBRATION_PARAM, "r", "r", baseLinePropertyAccessor);
    
    auto g435LeDisp2DepthPropertyAccessor = std::make_shared<G435LeDisp2DepthPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", g435LeDisp2DepthPropertyAccessor);      // hw
    propertyServer->registerProperty(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", g435LeDisp2DepthPropertyAccessor);  // sw
    propertyServer->registerProperty(OB_PROP_DEPTH_PRECISION_LEVEL_INT, "rw", "rw", g435LeDisp2DepthPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST, "r", "r", g435LeDisp2DepthPropertyAccessor);

    auto privatePropertyAccessor = std::make_shared<PrivateFilterPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, "rw", "rw", privatePropertyAccessor);
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    auto frameTransformPropertyAccessor = std::make_shared<StereoFrameTransformPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DEPTH_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor); // depth
    propertyServer->registerProperty(OB_PROP_DEPTH_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor); // color
    propertyServer->registerProperty(OB_PROP_COLOR_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_COLOR_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor); // left ir
    propertyServer->registerProperty(OB_PROP_IR_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_RIGHT_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor); // right ir
    propertyServer->registerProperty(OB_PROP_IR_RIGHT_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_RIGHT_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);

    BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_NET); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set device communication type to ethernet mode failed!"); })
}
void G435LeDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto              sensorType = sensor->getSensorType();
    auto              streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList streamProfileList;
    if(streamType == OB_STREAM_DEPTH) {
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 400, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 480, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 480, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 480, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 640, 480, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y16, 1280, 800, 10));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 400, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 480, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 480, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 480, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 640, 480, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_RVL, 1280, 800, 10));
    }
    else if(streamType == OB_STREAM_COLOR) {
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 720, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 720, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 720, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 720, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 800, 600, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 800, 600, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 800, 600, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 800, 600, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 360, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 360, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 360, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 360, 20));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 360, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 360, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 360, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 360, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 640, 400, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 800, 600, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 800, 600, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 800, 600, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 800, 600, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 1280, 720, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_YUYV, 1280, 720, 10));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 360, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 360, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 360, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 360, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 640, 400, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 800, 600, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 800, 600, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 800, 600, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 800, 600, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 1280, 720, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_I420, 1280, 720, 10));
    }
    else if(streamType == OB_STREAM_IR_LEFT) {
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 20));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 20));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y10, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y10, 1280, 800, 10));
    }
    else if(streamType == OB_STREAM_IR_RIGHT) {
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 1280, 800, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y8, 640, 400, 20));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 1280, 800, 20));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 10));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 15));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_MJPG, 640, 400, 20));

        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y10, 1280, 800, 5));
        streamProfileList.emplace_back(StreamProfileFactory::createVideoStreamProfile(streamType, OB_FORMAT_Y10, 1280, 800, 10));
    }
    if(!streamProfileList.empty()) {
        sensor->setStreamProfileList(streamProfileList);
    }
    G435LeDeviceBase::initSensorStreamProfile(sensor);
}

void G435LeDevice::fetchDeviceInfo() {
    auto portInfo          = enumInfo_->getSourcePortInfoList().front();
    auto netPortInfo       = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
    auto deviceInfo        = std::make_shared<NetDeviceInfo>();
    deviceInfo->ipAddress_ = netPortInfo->address;
    deviceInfo_            = deviceInfo;

    deviceInfo_->name_           = enumInfo_->getName();
    deviceInfo_->pid_            = enumInfo_->getPid();
    deviceInfo_->vid_            = enumInfo_->getVid();
    deviceInfo_->uid_            = enumInfo_->getUid();
    deviceInfo_->connectionType_ = enumInfo_->getConnectionType();

    auto propServer                   = getPropertyServer();
    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;

    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = "Orbbec " + deviceInfo_->name_;

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
    extensionInfo_["MCUVersion"]               = version.subSystemVersion;
}

}  // namespace libobsensor
