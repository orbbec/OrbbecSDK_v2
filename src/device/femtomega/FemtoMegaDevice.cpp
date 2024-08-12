#include "FemtoMegaDevice.hpp"
#include "Platform.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "FilterFactory.hpp"
#include "component/metadata/FrameMetadataParserContainer.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"
#include "component/property/VendorPropertyAccessor.hpp"
#include "component/property/UvcPropertyAccessor.hpp"
#include "component/property/PropertyServer.hpp"
#include "component/property/CommonPropertyAccessors.hpp"
#include "component/property/FilterPropertyAccessors.hpp"
#include "component/monitor/DeviceMonitor.hpp"

#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "gemini330/G330DeviceSyncConfigurator.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "param/AlgParamManager.hpp"
#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTSPStreamPort.hpp"
#endif

namespace libobsensor {
FemtoMegaDevice::FemtoMegaDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

FemtoMegaDevice::~FemtoMegaDevice() noexcept {}

void FemtoMegaDevice::init() {
    std::string connectType = enumInfo_->getConnectionType();
    if(connectType == "Ethernet") {
        initNetModeSensorList();
        initNetModeProperties();
        fetchDeviceInfo();
        fetchNetModeAllProfileList();
    }
    else {
        initSensorList();
        initProperties();
        fetchDeviceInfo();
    }

    // auto GlobalTimestampFilter = std::make_shared<GlobalTimestampFilter>(this);
    // registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, GlobalTimestampFilter);

    auto algParamManager = std::make_shared<TOFDeviceCommandAlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    // static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes     = { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE,
    //                                                                            OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED
    //                                                                            };
    // auto                                            deviceSyncConfigurator = std::make_shared<G330DeviceSyncConfigurator>(this, supportedSyncModes);
    // registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);
}

void FemtoMegaDevice::fetchDeviceInfo() {
    auto propServer = getPropertyServer();

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

    if(deviceInfo_->name_.find("Orbbec") == std::string::npos) {
        deviceInfo_->name_ = "" + deviceInfo_->name_;
    }
}

void FemtoMegaDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto sensorType    = sensor->getSensorType();
    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<TOFDeviceCommandAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }

    // sensor->registerStreamStateChangedCallback([this](OBStreamState state, const std::shared_ptr<const StreamProfile> &sp) {
    //     auto streamStrategy = getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY);
    //     if(state == STREAM_STATE_STARTING) {
    //         streamStrategy->markStreamStarted(sp);
    //     }
    //     else if(state == STREAM_STATE_STOPPED) {
    //         streamStrategy->markStreamStopped(sp);
    //     }
    // });
}

void FemtoMegaDevice::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });
    auto        platform           = Platform::getInstance();
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == 2;
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;

        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(depthPortInfo);

                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_DEPTH, port);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                // auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                // if(frameProcessor) {
                //     sensor->setFrameProcessor(frameProcessor.get());
                // }

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);

        registerSensorPortInfo(OB_SENSOR_DEPTH, depthPortInfo);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });

        // the main property accessor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, depthPortInfo]() {
            auto platform = Platform::getInstance();
            auto port     = platform->getSourcePort(depthPortInfo);
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });
    }

    auto irPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == 4;
    });

    if(irPortInfoIter != sourcePortInfoList.end()) {
        auto irPortInfo = *irPortInfoIter;

        registerComponent(
            OB_DEV_COMPONENT_IR_SENSOR,
            [this, irPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(irPortInfo);

                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);

        registerSensorPortInfo(OB_SENSOR_IR, irPortInfo);

        registerComponent(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR);
            return frameProcessor;
        });
    }

    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == 0;
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
                    // { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    //  { FormatFilterPolicy::REPLACE, OB_FORMAT_BYR2, OB_FORMAT_RW16, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGB, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGBA, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGRA, formatConverter });
                    // formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y16, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y8, formatConverter });
                }

                sensor->updateFormatFilterConfig(formatFilterConfigs);
                sensor->setFrameMetadataParserContainer(colorMdParserContainer_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

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
        return portInfo->portType == SOURCE_PORT_USB_HID; 
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

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void FemtoMegaDevice::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

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
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_DEPTH) {
            auto vendorPropertyAccessor = std::make_shared<LazyExtensionPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMESTAMP_OFFSET_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_SOFT_FILTER_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_MAX_DIFF_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_MAX_SPECKLE_SIZE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_SWITCH_IR_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_SPEED_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_USB_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DC_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
            //  propertyServer->registerProperty(OB_STRUCT_PERIPHERAL_ID_INFO, "", "r", vendorPropertyAccessor);
            //  propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_CAMERA_CALIB_JSON_FILE, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_RAW_DATA_MCU_UPGRADE_FILE, "rw", "rw", vendorPropertyPort);
            // propertyServer->registerProperty(OB_RAW_DATA_HARDWARE_ALIGN_PARAM, "rw", "rw", vendorPropertyPort);
            propertyServer->registerProperty(OB_PROP_INDICATOR_LIGHT_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyPort);

            propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_EXPOSURE_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_PROP_FAN_MAX_SPEED_TEST_MODE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
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

    propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_DEPTH_EXPOSURE_INT);

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

void FemtoMegaDevice::initNetModeSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    auto        platform           = Platform::getInstance();
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();

    auto vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_VENDOR;
    });

    std::shared_ptr<const SourcePortInfo> vendorPortInfo;
    if(vendorPortInfoIter != sourcePortInfoList.end()) {
        vendorPortInfo = *vendorPortInfoIter;
        registerSensorPortInfo(OB_SENSOR_DEPTH, vendorPortInfo);
        registerSensorPortInfo(OB_SENSOR_IR, vendorPortInfo);
        registerSensorPortInfo(OB_SENSOR_COLOR, vendorPortInfo);
    }
    
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_DEPTH;
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(depthPortInfo);
                
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_DEPTH, port);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                initNetModeSensorStreamProfileList(sensor);
                return sensor;
            },
            true);

        registerComponent(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_DEPTH);
            return frameProcessor;
        });

        // the main property accessor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, vendorPortInfo]() {
            auto platform = Platform::getInstance();
            auto port     = platform->getSourcePort(vendorPortInfo);
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });
    }

    auto irPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_NET_RTSP && std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo)->streamType == OB_STREAM_IR;
    });

    if(irPortInfoIter != sourcePortInfoList.end()) {
        auto irPortInfo = *irPortInfoIter;

        registerComponent(
            OB_DEV_COMPONENT_IR_SENSOR,
            [this, irPortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(irPortInfo);

                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                initNetModeSensorStreamProfileList(sensor);
                return sensor;
            },
            true);

        registerComponent(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, [this]() {
            auto factory = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);

            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR);
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
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    // { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    //  { FormatFilterPolicy::REPLACE, OB_FORMAT_BYR2, OB_FORMAT_RW16, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGB, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_RGBA, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_BGRA, formatConverter });
                    // formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y16, formatConverter });
                    //  formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_YUYV, OB_FORMAT_Y8, formatConverter });
                }

                sensor->updateFormatFilterConfig(formatFilterConfigs);
                sensor->setFrameMetadataParserContainer(colorMdParserContainer_);
                sensor->setFrameTimestampCalculator(videoFrameTimestampCalculator_);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initNetModeSensorStreamProfileList(sensor);

                return sensor;
            },
            true);

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

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void FemtoMegaDevice::initNetModeProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);
    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto  platform       = Platform::getInstance();
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR) {
            //TODO::LazyExtensionPropertyAccessor？
            auto vendorPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", vendorPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_DEPTH) {
            auto vendorPropertyAccessor = std::make_shared<LazyExtensionPropertyAccessor>([this, &sourcePortInfo]() {
                auto platform = Platform::getInstance();
                auto port     = platform->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMESTAMP_OFFSET_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_SOFT_FILTER_BOOL, "rw", "rw", vendorPropertyAccessor); //
            propertyServer->registerProperty(OB_PROP_DEPTH_MAX_DIFF_INT, "rw", "rw", vendorPropertyAccessor); //
            propertyServer->registerProperty(OB_PROP_DEPTH_MAX_SPECKLE_SIZE_INT, "rw", "rw", vendorPropertyAccessor);//
            propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_SWITCH_IR_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_FAN_WORK_SPEED_INT, "rw", "rw", vendorPropertyAccessor);
            //TODO: ADD?
            //propertyServer->registerProperty(OB_PROP_USB_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
            //propertyServer->registerProperty(OB_PROP_DC_POWER_STATE_INT, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_IP_ADDR_CONFIG, "rw", "rw", vendorPropertyAccessor);
            //  propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_CAMERA_CALIB_JSON_FILE, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
            //propertyServer->registerProperty(OB_RAW_DATA_MCU_UPGRADE_FILE, "rw", "rw", vendorPropertyPort);
            //propertyServer->registerProperty(OB_RAW_DATA_HARDWARE_ALIGN_PARAM, "rw", "rw", vendorPropertyPort);
            propertyServer->registerProperty(OB_PROP_INDICATOR_LIGHT_BOOL, "rw", "rw", vendorPropertyAccessor);
            //TODO::firmVer >= 10201
            propertyServer->registerProperty(OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, "w", "w", vendorPropertyAccessor);
            //TODO:;firmVer >= 10202
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_STRUCT_LED_CONTROL, "", "w", vendorPropertyPort);

            //propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_EXPOSURE_INT, "r", "r", vendorPropertyAccessor);
            //propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_PROP_FAN_MAX_SPEED_TEST_MODE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_ODR_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_ODR_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_FULL_SCALE_INT, "rw", "rw", vendorPropertyAccessor);

            //TODO:: Support？
            propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_ODR_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_GET_GYRO_PRESETS_FULL_SCALE_LIST, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_ACCEL_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_GYRO_SWITCH_BOOL, "", "rw", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_RAW_DATA_STREAM_PROFILE_LIST, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "r", vendorPropertyAccessor);

            // auto        propServer = getPropertyServer();
            // auto        version    = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
            // std::string fwVersion  = version.firmwareVersion;
            // int firmVer = 0;
            // utils::getFirmwareVersionInt(fwVersion, firmVer);
            // if(firmVer >= 10107) {
            //     //propertyServer->registerProperty(OB_RAW_DATA_DEVICE_UPGRADE, "", "rw", vendorPropertyAccessor);
            //     //propertyServer->registerProperty(OB_STRUCT_DEVICE_UPGRADE_STATUS, "", "rw", vendorPropertyAccessor);                                    
            // }
            // if(firmVer >= 10201) {                                   
            //     propertyServer->registerProperty(OB_PROP_RESTORE_FACTORY_SETTINGS_BOOL, "w", "w", vendorPropertyAccessor);
            //     propertyServer->registerProperty(OB_PROP_DEVICE_IN_RECOVERY_MODE_BOOL, "r", "r", vendorPropertyAccessor);
            //     propertyServer->registerProperty(OB_PROP_DEVICE_DEVELOPMENT_MODE_INT, "rw", "rw", vendorPropertyAccessor);

            // }
            // if(firmVer >= 10202) {
            //     propertyServer->registerProperty(OB_PROP_TIMER_RESET_ENABLE_BOOL, "rw", "rw", vendorPropertyAccessor);
            //     propertyServer->registerProperty(OB_PROP_TIMER_RESET_SIGNAL_BOOL, "w", "w", vendorPropertyAccessor);
            // }
            // if(firmVer >= 10203) {
            //     propertyServer->registerProperty(OB_STRUCT_DEVICE_STATIC_IP_CONFIG_RECORD, "rw", "rw", vendorPropertyAccessor);
            // }
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

    propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_DEPTH_EXPOSURE_INT);

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

void FemtoMegaDevice::initNetModeSensorStreamProfileList(std::shared_ptr<ISensor> sensor) {
    auto sensorType    = sensor->getSensorType();
    OBStreamType streamType = utils::mapSensorTypeToStreamType(sensorType);
    StreamProfileList ProfileList;
    for (const auto& profile : allNetProfileList_) {
        if(streamType == profile->getType()){
            ProfileList.push_back(profile);
        }
    }
    
    sensor->updateStreamProfileList(ProfileList);

    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensorType);
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<TOFDeviceCommandAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }
}

void FemtoMegaDevice::fetchNetModeAllProfileList() {
    auto propServer = getPropertyServer();
    BEGIN_TRY_EXECUTE({
        propServer->setPropertyValueT(OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, OB_COMM_NET);
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set device ethernet mode failed!"); })

    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            OB_RAW_DATA_STREAM_PROFILE_LIST,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get profile list params failed!");
        data.clear();
    })

    if(!data.empty()) {
        std::vector<OBInternalStreamProfile> outputProfiles;
        uint16_t                             dataSize = static_cast<uint16_t>(data.size());
        outputProfiles                                = parseBuffer<OBInternalStreamProfile>(data.data(), dataSize);
        allNetProfileList_.clear();
        for(auto item: outputProfiles) {
            OBStreamType streamType = utils::mapSensorTypeToStreamType((OBSensorType)item.sensorType);
            OBFormat     format     = utils::uvcFourccToOBFormat(item.profile.video.formatFourcc);
            allNetProfileList_.push_back(StreamProfileFactory::createVideoStreamProfile(streamType, format, item.profile.video.width, item.profile.video.height,
                                                                                     item.profile.video.fps));
        }
    }
    else {
        LOG_WARN("Get stream profile list failed!");
    }
}

std::vector<std::shared_ptr<IFilter>> FemtoMegaDevice::createRecommendedPostProcessingFilters(OBSensorType type) {
    std::vector<std::shared_ptr<IFilter>> filters;
    if(type == OB_SENSOR_COLOR) {
        return {};
    }
    return filters;
}
}  // namespace libobsensor