#include "G330Device.hpp"

#include "DevicePids.hpp"
#include "InternalTypes.hpp"

#include "Platform.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "monitor/DeviceMonitor.hpp"

#include "G330MetadataParser.hpp"
#include "G330MetadataTypes.hpp"
#include "G330DeviceSyncConfigurator.hpp"
#include "G330AlgParamManager.hpp"
#include "G330PresetManager.hpp"
#include "G330DepthWorkModeManager.hpp"
#include "G330SensorStreamStrategy.hpp"
#include "G330PropertyAccessors.hpp"

#include <algorithm>

namespace libobsensor {

constexpr uint8_t INTERFACE_COLOR = 4;
constexpr uint8_t INTERFACE_DEPTH = 0;

G330Device::G330Device(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

G330Device::~G330Device() noexcept {}

void G330Device::init() {
    initSensorList();
    initProperties();
    initFrameMetadataParserContainer();

    fetchDeviceInfo();

    videoFrameTimestampCalculatorCreator_ = [this]() {
        auto metadataType = OB_FRAME_METADATA_TYPE_TIMESTAMP;
        auto iter         = std::find(G330LDevPids.begin(), G330LDevPids.end(), deviceInfo_->pid_);
        if(iter != G330LDevPids.end()) {
            metadataType = OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP;
        }
        return std::make_shared<FrameTimestampCalculatorOverMetadata>(this, metadataType, frameTimeFreq_);
    };

    auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
    registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

    auto algParamManager = std::make_shared<G330AlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    auto depthWorkModeManager = std::make_shared<G330DepthWorkModeManager>(this);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);

    auto presetManager = std::make_shared<G330PresetManager>(this);
    registerComponent(OB_DEV_COMPONENT_PRESET_MANAGER, presetManager);

    auto sensorStreamStrategy = std::make_shared<G330SensorStreamStrategy>(this);
    registerComponent(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, sensorStreamStrategy);

    static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes = {
        OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN,         OB_MULTI_DEVICE_SYNC_MODE_STANDALONE,          OB_MULTI_DEVICE_SYNC_MODE_PRIMARY,
        OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING
    };
    auto deviceSyncConfigurator = std::make_shared<G330DeviceSyncConfigurator>(this, supportedSyncModes);
    registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);
}

void G330Device::fetchDeviceInfo() {
    auto propServer                   = getPropertyServer();
    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    deviceInfo_                       = std::make_shared<DeviceInfo>();
    deviceInfo_->name_                = version.deviceName;
    deviceInfo_->fwVersion_           = version.firmwareVersion;
    deviceInfo_->deviceSn_            = version.serialNumber;
    deviceInfo_->asicName_            = version.depthChip;
    deviceInfo_->hwVersion_           = version.hardwareVersion;
    deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
    deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
    deviceInfo_->pid_                 = enumInfo_->getPid();
    deviceInfo_->vid_                 = enumInfo_->getVid();
    deviceInfo_->uid_                 = enumInfo_->getUid();
    deviceInfo_->connectionType_      = enumInfo_->getConnectionType();
    // todo: fetch and parse extension info
}

void G330Device::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensor->getSensorType());
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<G330AlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    auto sensorType = sensor->getSensorType();
    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }
}

void G330Device::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    auto        platform           = Platform::getInstance();
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_DEPTH;
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<DisparityBasedSensor>(this, OB_SENSOR_DEPTH, port);

                sensor->updateFormatFilterConfig({ { FormatFilterPolicy::REMOVE, OB_FORMAT_Y8, OB_FORMAT_ANY, nullptr },
                                                   { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                                                   { FormatFilterPolicy::REMOVE, OB_FORMAT_BA81, OB_FORMAT_ANY, nullptr },
                                                   { FormatFilterPolicy::REMOVE, OB_FORMAT_YV12, OB_FORMAT_ANY, nullptr },
                                                   { FormatFilterPolicy::REMOVE, OB_FORMAT_UYVY, OB_FORMAT_ANY, nullptr },
                                                   { FormatFilterPolicy::REPLACE, OB_FORMAT_Z16, OB_FORMAT_Y16, nullptr } });

                sensor->setFrameMetadataParserContainer(depthMdParserContainer_);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                auto propServer = getPropertyServer();
                auto depthUnit  = propServer->getPropertyValueT<float>(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT);
                sensor->setDepthUnit(depthUnit);

                auto hwD2D = propServer->getPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL);
                sensor->markOutputDisparityFrame(!hwD2D);

                initSensorStreamProfile(sensor);

                sensor->registerStreamStateChangedCallback([&](OBStreamState state, const std::shared_ptr<const StreamProfile> &sp) {
                    if(state == STREAM_STATE_STREAMING) {
                        auto algParamManager = getComponentT<G330AlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
                        algParamManager->reFetchDisparityParams();
                        algParamManager->bindDisparityParam({ sp });
                    }
                });

                return sensor;
            },
            true);

        registerSensorPortInfo(OB_SENSOR_DEPTH, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });

        registerComponent(
            OB_DEV_COMPONENT_LEFT_IR_SENSOR,
            [this, depthPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_LEFT, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_Z16, OB_FORMAT_ANY, nullptr },  //
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BA81, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_YV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_NV12, OB_FORMAT_Y12, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FrameUnpacker", OB_SENSOR_IR_LEFT, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::REPLACE, OB_FORMAT_NV12, OB_FORMAT_Y16, formatConverter });
                }

                sensor->updateFormatFilterConfig(formatFilterConfigs);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer_);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
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
        registerSensorPortInfo(OB_SENSOR_IR_LEFT, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_LEFT);
            return frameProcessor;
        });

        registerComponent(
            OB_DEV_COMPONENT_RIGHT_IR_SENSOR,
            [this, depthPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR_RIGHT, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_Z16, OB_FORMAT_ANY, nullptr },   //
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_Y8, OB_FORMAT_ANY, nullptr },    //
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },  //
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_UYVY, OB_FORMAT_ANY, nullptr },  //
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_BA81, OB_FORMAT_Y8, nullptr },  //
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_YV12, OB_FORMAT_Y12, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FrameUnpacker", OB_SENSOR_IR_RIGHT, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::REPLACE, OB_FORMAT_YV12, OB_FORMAT_Y16, formatConverter });
                }

                sensor->updateFormatFilterConfig(formatFilterConfigs);
                sensor->setFrameMetadataParserContainer(depthMdParserContainer_);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
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
        registerSensorPortInfo(OB_SENSOR_IR_RIGHT, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR_RIGHT);
            return frameProcessor;
        });

        // the main property accessor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, depthPortInfo]() {
            auto platform      = Platform::getInstance();
            auto port          = platform->getSourcePort(depthPortInfo);
            auto uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            uvcDevicePort->updateXuUnit(OB_G330_XU_UNIT);  // update xu unit to g330 xu unit
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        // The device monitor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, depthPortInfo]() {
            auto platform      = Platform::getInstance();
            auto port          = platform->getSourcePort(depthPortInfo);
            auto uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(port);
            uvcDevicePort->updateXuUnit(OB_G330_XU_UNIT);  // update xu unit to g330 xu unit
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_COLOR;
    });

    if(colorPortInfoIter != sourcePortInfoList.end()) {
        auto colorPortInfo = *colorPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_COLOR_SENSOR,
            [this, colorPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_BYR2, OB_FORMAT_RW16, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGBA, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGRA, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y16, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y8, formatConverter });
                }

                sensor->updateFormatFilterConfig(formatFilterConfigs);
                sensor->setFrameMetadataParserContainer(colorMdParserContainer_);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
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
        return portInfo->portType == SOURCE_PORT_USB_HID;  //
    });

    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter
            auto platform           = Platform::getInstance();
            auto port               = platform->getSourcePort(imuPortInfo);
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL);
            if(!imuCorrectorFilter) {
                throw not_implemented_exception("Cannot find IMU correcter filter!");
            }
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto imuStreamer    = std::make_shared<ImuStreamer>(this, dataStreamPort, imuCorrectorFilter);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto platform             = Platform::getInstance();
                auto port                 = platform->getSourcePort(imuPortInfo);
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
                auto platform             = Platform::getInstance();
                auto port                 = platform->getSourcePort(imuPortInfo);
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

void G330Device::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto d2dPropertyAccessor = std::make_shared<G330Disp2DepthPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", d2dPropertyAccessor);      // hw
    propertyServer->registerProperty(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", d2dPropertyAccessor);  // sw
    propertyServer->registerProperty(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT, "rw", "rw", d2dPropertyAccessor);

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto  platform       = Platform::getInstance();
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);  // replace by vendor property accessor
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_HUE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAMMA_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, "rw", "rw", uvcPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_DEPTH) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });
            propertyServer->registerProperty(OB_PROP_DEPTH_GAIN_INT, "rw", "rw", uvcPropertyAccessor);

            auto vendorPropertyAccessor = std::make_shared<LazyExtensionPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform      = Platform::getInstance();
                auto port          = platform->getSourcePort(sourcePortInfo);
                auto uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(port);
                uvcDevicePort->updateXuUnit(OB_G330_XU_UNIT);  // update xu unit to g330 xu unit
                auto vendorPropertyAccessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return vendorPropertyAccessor;
            });

            propertyServer->registerProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);  // using vendor property accessor
            propertyServer->registerProperty(OB_PROP_LDP_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_CONTROL_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_ALWAYS_ON_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_ON_OFF_PATTERN_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TEMPERATURE_COMPENSATION_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LDP_STATUS_BOOL, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LDP_MEASURE_DISTANCE_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_DELAY_US_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_VERSION, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_ASIC_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_DEPTH_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEPTH_HDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_COLOR_AE_ROI, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEPTH_AE_ROI, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);

            // todo: add these properties to the frame processor
            // propertyServer->registerProperty(OB_PROP_SDK_DEPTH_FRAME_UNPACK_BOOL, "rw", "rw", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "rw", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_PROP_GPM_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_POWER_ACTUAL_LEVEL_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "", "rw", vendorPropertyAccessor);
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
            propertyServer->registerProperty(OB_PROP_IR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_USB2_REPEAT_IDENTIFY_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_DEVICE_EXTENSION_INFORMATION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_IR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DISP_SEARCH_RANGE_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_SLAVE_DEVICE_SYNC_STATUS_BOOL, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_RESET_BOOL, "", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_DEPTH_ALG_MODE_LIST, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "", "rw", vendorPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_ACCEL) {
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", sensor);
            if(imuCorrectorFilter) {
                auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
                propertyServer->registerProperty(OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
            }
        }
        else if(sensor == OB_SENSOR_GYRO) {
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", sensor);
            if(imuCorrectorFilter) {
                auto filterStateProperty = std::make_shared<FilterStatePropertyAccessor>(imuCorrectorFilter);
                propertyServer->registerProperty(OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL, "rw", "rw", filterStateProperty);
            }
        }
    }

    propertyServer->aliasProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
    propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_DEPTH_EXPOSURE_INT);
    propertyServer->aliasProperty(OB_PROP_IR_GAIN_INT, OB_PROP_DEPTH_GAIN_INT);

    auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);

    auto baseLinePropertyAccessor = std::make_shared<BaselinePropertyAccessor>(this);
    propertyServer->registerProperty(OB_STRUCT_BASELINE_CALIBRATION_PARAM, "r", "r", baseLinePropertyAccessor);

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

// todo: refactor this as component
void G330Device::initFrameMetadataParserContainer() {
    // for depth and left/right ir sensor
    depthMdParserContainer_ = std::make_shared<FrameMetadataParserContainer>();
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330MetadataTimestampParser<G330DepthUvcMetadata>>());
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP, std::make_shared<G330MetadataSensorTimestampParser>());
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G330DepthUvcMetadata::frame_counter));
    // todo: calculate actual fps according exposure and frame rate
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G330DepthUvcMetadata::actual_fps));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G330DepthUvcMetadata::gain_level));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                                            makeStructureMetadataParser(&G330CommonUvcMetadata::bitmap_union_0,
                                                                        [](const uint64_t &param) {  //
                                                                            return ((G330ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                                        }));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G330CommonUvcMetadata::exposure));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_priority));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_power));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_power_level));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_LASER_STATUS, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_status));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_left));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_top));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_right));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_bottom));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA, makeStructureMetadataParser(&G330DepthUvcMetadata::gpio_input_data));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_name));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_size));
    depthMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_id));

    // for color sensor
    colorMdParserContainer_ = std::make_shared<FrameMetadataParserContainer>();
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330MetadataTimestampParser<G330ColorUvcMetadata>>());
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                                            std::make_shared<G330ColorMetadataSensorTimestampParser>([](const int64_t &param) { return param * 100; }));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G330CommonUvcMetadata::frame_counter));
    // todo: calculate actual fps according exposure and frame rate
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G330ColorUvcMetadata::actual_fps));

    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                                            makeStructureMetadataParser(&G330CommonUvcMetadata::bitmap_union_0,
                                                                        [](const int64_t &param) {  //
                                                                            return ((G330ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                                        }));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G330CommonUvcMetadata::exposure));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G330ColorUvcMetadata::gain_level));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, makeStructureMetadataParser(&G330ColorUvcMetadata::auto_white_balance));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, makeStructureMetadataParser(&G330ColorUvcMetadata::white_balance));
    // colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_MANUAL_WHITE_BALANCE, makeStructureMetadataParser(&G330ColorUvcMetadata::white_balance));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, makeStructureMetadataParser(&G330ColorUvcMetadata::brightness));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, makeStructureMetadataParser(&G330ColorUvcMetadata::contrast));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_SATURATION, makeStructureMetadataParser(&G330ColorUvcMetadata::saturation));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, makeStructureMetadataParser(&G330ColorUvcMetadata::sharpness));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION,
                                            makeStructureMetadataParser(&G330ColorUvcMetadata::backlight_compensation));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_GAMMA, makeStructureMetadataParser(&G330ColorUvcMetadata::gamma));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_HUE, makeStructureMetadataParser(&G330ColorUvcMetadata::hue));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY,
                                            makeStructureMetadataParser(&G330ColorUvcMetadata::power_line_frequency));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION,
                                            makeStructureMetadataParser(&G330ColorUvcMetadata::low_light_compensation));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_left));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_top));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_right));
    colorMdParserContainer_->registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_bottom));
}

std::vector<std::shared_ptr<IFilter>> G330Device::createRecommendedPostProcessingFilters(OBSensorType type) {
    auto filterFactory = FilterFactory::getInstance();
    if(type == OB_SENSOR_DEPTH) {
        std::vector<std::shared_ptr<IFilter>> depthFilterList;

        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            depthFilterList.push_back(decimationFilter);
        }

        if(filterFactory->isFilterCreatorExists("NoiseRemovalFilter")) {
            auto noiseFilter = filterFactory->createFilter("NoiseRemovalFilter");
            // max_size, min_diff, width, height
            std::vector<std::string> params = { "80", "256", "848", "480" };
            noiseFilter->updateConfig(params);
            depthFilterList.push_back(noiseFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
            // magnitude, alpha, disp_diff, radius
            std::vector<std::string> params = { "1", "0.5", "160", "1" };
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
            auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
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
        std::vector<std::shared_ptr<IFilter>> colorFilterList;
        if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
            auto decimationFilter = filterFactory->createFilter("DecimationFilter");
            decimationFilter->enable(false);
            colorFilterList.push_back(decimationFilter);
        }
        return colorFilterList;
    }

    return {};
}
}  // namespace libobsensor