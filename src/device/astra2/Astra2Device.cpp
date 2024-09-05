#include "Astra2Device.hpp"

#include "DevicePids.hpp"
#include "InternalTypes.hpp"

#include "Platform.hpp"
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
#include "monitor/DeviceMonitor.hpp"

#include "Astra2AlgParamManager.hpp"
#include "Astra2StreamProfileFilter.hpp"
#include "Astra2PropertyAccessors.hpp"
#include "Astra2DepthWorkModeManager.hpp"
#include "Astra2FrameTimestampCalculator.hpp"
#include "Astra2DeviceSyncConfigurator.hpp"

#include <algorithm>

namespace libobsensor {

constexpr uint8_t INTERFACE_COLOR = 4;
constexpr uint8_t INTERFACE_IR    = 2;
constexpr uint8_t INTERFACE_DEPTH = 0;

Astra2Device::Astra2Device(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

Astra2Device::~Astra2Device() noexcept {}

void Astra2Device::init() {
    initSensorList();
    initProperties();

    fetchDeviceInfo();
    fetchExtensionInfo();

    videoFrameTimestampCalculatorCreator_ = [this]() { return std::make_shared<Astra2VideoFrameTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_); };

    auto globalTimestampFilter = std::make_shared<GlobalTimestampFitter>(this);
    registerComponent(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, globalTimestampFilter);

    auto algParamManager = std::make_shared<Astra2AlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    auto depthWorkModeManager = std::make_shared<Astra2DepthWorkModeManager>(this);
    registerComponent(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER, depthWorkModeManager);

    // auto sensorStreamStrategy = std::make_shared<Astra2SensorStreamStrategy>(this);
    // registerComponent(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, sensorStreamStrategy);

    static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes     = { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE,
                                                                               OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED };
    auto                                            deviceSyncConfigurator = std::make_shared<Astra2DeviceSyncConfigurator>(this, supportedSyncModes);
    registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);

    auto deviceClockSynchronizer = std::make_shared<DeviceClockSynchronizer>(this, deviceTimeFreq_, 1000);
    registerComponent(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER, deviceClockSynchronizer);
}

void Astra2Device::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {

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

    // sensor->registerStreamStateChangedCallback([this](OBStreamState state, const std::shared_ptr<const StreamProfile> &sp) {
    //     auto streamStrategy = getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY);
    //     if(state == STREAM_STATE_STARTING) {
    //         streamStrategy->markStreamActivated (sp);
    //     }
    //     else if(state == STREAM_STATE_STOPPED) {
    //         streamStrategy->markStreamDeactivated(sp);
    //     }
    // });
}

void Astra2Device::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    registerComponent(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER, [this]() { return std::make_shared<Astra2StreamProfileFilter>(this); });

    auto        pal                = Platform::getInstance();
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_DEPTH;
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto pal    = Platform::getInstance();
                auto port   = pal->getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<DisparityBasedSensor>(this, OB_SENSOR_DEPTH, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_MJPG, OB_FORMAT_RLE, nullptr },
                };
                auto formatConverter = getSensorFrameFilter("FrameUnpacker", OB_SENSOR_DEPTH, false);
                if(formatConverter) {
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_Y16, formatConverter });
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                auto propServer = getPropertyServer();

                propServer->setPropertyValueT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL, false);
                propServer->setPropertyValueT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, true);
                sensor->markOutputDisparityFrame(true);

                propServer->setPropertyValueT(OB_PROP_DEPTH_PRECISION_LEVEL_INT, OB_PRECISION_1MM);
                sensor->setDepthUnit(1.0f);

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

        // the main property accessor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, depthPortInfo]() {
            auto pal      = Platform::getInstance();
            auto port     = pal->getSourcePort(depthPortInfo);
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        // The device monitor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, depthPortInfo]() {
            auto pal        = Platform::getInstance();
            auto port       = pal->getSourcePort(depthPortInfo);
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    auto irPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_IR;
    });

    if(irPortInfoIter != sourcePortInfoList.end()) {
        auto irPortInfo = *irPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_IR_SENSOR,
            [this, irPortInfo]() {
                auto pal    = Platform::getInstance();
                auto port   = pal->getSourcePort(irPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                initSensorStreamProfile(sensor);

                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_IR, irPortInfo);

        registerComponent(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR, [this]() {
            auto factory        = getComponentT<FrameProcessorFactory>(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY);
            auto frameProcessor = factory->createFrameProcessor(OB_SENSOR_IR);
            return frameProcessor;
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
                auto pal    = Platform::getInstance();
                auto port   = pal->getSourcePort(colorPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_COLOR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV12, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REPLACE, OB_FORMAT_BYR2, OB_FORMAT_RW16, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                if(formatConverter) {
#ifdef WIN32
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_BGR, OB_FORMAT_RGB, formatConverter });
#else
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_RGB, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGR, formatConverter });
                    formatFilterConfigs.push_back({ FormatFilterPolicy::ADD, OB_FORMAT_MJPG, OB_FORMAT_BGRA, formatConverter });
#endif
                }
                sensor->updateFormatFilterConfig(formatFilterConfigs);

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
            auto pal                = Platform::getInstance();
            auto port               = pal->getSourcePort(imuPortInfo);
            auto imuCorrectorFilter = getSensorFrameFilter("IMUCorrector", OB_SENSOR_ACCEL, true);

            imuCorrectorFilter->enable(false);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto imuStreamer    = std::make_shared<ImuStreamer>(this, dataStreamPort, imuCorrectorFilter);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto pal                  = Platform::getInstance();
                auto port                 = pal->getSourcePort(imuPortInfo);
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
                auto pal                  = Platform::getInstance();
                auto port                 = pal->getSourcePort(imuPortInfo);
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

void Astra2Device::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto d2dPropertyAccessor = std::make_shared<Astra2Disp2DepthPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", d2dPropertyAccessor);      // hw
    propertyServer->registerProperty(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, "rw", "rw", d2dPropertyAccessor);  // sw
    propertyServer->registerProperty(OB_PROP_DEPTH_PRECISION_LEVEL_INT, "rw", "rw", d2dPropertyAccessor);

    auto tempPropertyAccessor = std::make_shared<Astra2TempPropertyAccessor>(this);
    propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", tempPropertyAccessor);

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto  pal            = Platform::getInstance();
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([&sourcePortInfo]() {
                auto pal      = Platform::getInstance();
                auto port     = pal->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            // propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);  // replace by vendor property accessor
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_IR) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([sourcePortInfo]() {
                auto pal      = Platform::getInstance();
                auto port     = pal->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });
            propertyServer->registerProperty(OB_PROP_IR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
        }
        else if(sensor == OB_SENSOR_DEPTH) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([&sourcePortInfo]() {
                auto pal      = Platform::getInstance();
                auto port     = pal->getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, &sourcePortInfo]() {
                auto pal                    = Platform::getInstance();
                auto port                   = pal->getSourcePort(sourcePortInfo);
                auto vendorPropertyAccessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return vendorPropertyAccessor;
            });

            propertyServer->registerProperty(OB_PROP_IR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor);  // using vendor property accessor
            propertyServer->registerProperty(OB_PROP_LDP_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_MIN_DEPTH_INT, "rw", "rw", vendorPropertyAccessor);  // todo: map to d2d
            propertyServer->registerProperty(OB_PROP_MAX_DEPTH_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_HOLEFILTER_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LDP_STATUS_BOOL, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_LASER_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_BRT_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEVICE_WORK_MODE_INT, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL, "", "r", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST, "", "r", vendorPropertyAccessor);
            // OB_RAW_DATA_DISPARITY_TO_DEPTH_PROFILE_LIST // todo: implemented it

            propertyServer->registerProperty(OB_PROP_WATCHDOG_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TIME, "rw", "rw", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor);

            propertyServer->registerProperty(OB_RAW_DATA_DEPTH_ALG_MODE_LIST, "r", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_DEPTH_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, "", "r", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_SDK_DEPTH_FRAME_UNPACK_BOOL, "", "rw", vendorPropertyAccessor);
            // propertyServer->registerProperty(OB_PROP_DEPTH_RM_FILTER_BOOL, "rw", "rw", vendorPropertyAccessor);
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
            propertyServer->registerProperty(OB_PROP_DEVICE_RESET_BOOL, "", "w", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_STOP_IR_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_STOP_COLOR_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL, "rw", "rw", vendorPropertyAccessor);

            auto depthPrecisionSupportListAccessor = std::make_shared<StructureDataOverV1_1Accessor>(vendorPropertyAccessor, static_cast<uint16_t>(0));
            propertyServer->registerProperty(OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST, "r", "r", depthPrecisionSupportListAccessor);
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

    propertyServer->aliasProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PROP_IR_AUTO_EXPOSURE_BOOL);
    propertyServer->aliasProperty(OB_PROP_DEPTH_GAIN_INT, OB_PROP_IR_GAIN_INT);
    propertyServer->aliasProperty(OB_PROP_DEPTH_EXPOSURE_INT, OB_PROP_IR_EXPOSURE_INT);

    auto heartbeatPropertyAccessor = std::make_shared<HeartbeatPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartbeatPropertyAccessor);

    auto baseLinePropertyAccessor = std::make_shared<BaselinePropertyAccessor>(this);
    propertyServer->registerProperty(OB_STRUCT_BASELINE_CALIBRATION_PARAM, "r", "r", baseLinePropertyAccessor);

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

std::vector<std::shared_ptr<IFilter>> Astra2Device::createRecommendedPostProcessingFilters(OBSensorType type) {
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
            auto thresholdFilter = filterFactory->createFilter("ThresholdFilter");
            depthFilterList.push_back(thresholdFilter);
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
