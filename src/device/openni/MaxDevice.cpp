// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxDevice.hpp"
#include "DevicePids.hpp"
#include "InternalTypes.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "FilterFactory.hpp"
#include "OpenNIPropertyAccessors.hpp"
#include "OpenNIAlgParamManager.hpp"
#include "OpenNIStreamProfileFilter.hpp"
#include "MaxDisparitySensor.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "property/PrivateFilterPropertyAccessors.hpp"
#include "monitor/DeviceMonitor.hpp"
#include "OpenNIDeviceSyncConfigurator.hpp"
#include "DevicePids.hpp"
#include <algorithm>


namespace libobsensor {

constexpr uint8_t  INTERFACE_COLOR     = 0;
constexpr uint8_t  INTERFACE_DEPTH     = 0;
constexpr uint16_t MAX_PRO_COLOR_PID   = 0x0560;
constexpr uint16_t GEMINI_UW_COLOR_PID = 0x05aa;

MaxDevice::MaxDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : OpenNIDeviceBase(info) {
    LOG_INFO("Create {} device.", info->getName());
    init();
}

MaxDevice::~MaxDevice() noexcept {
    LOG_INFO("Destroy {} device.", deviceInfo_->name_);
}

void MaxDevice::init() {
    OpenNIDeviceBase::init();
    if(deviceInfo_->pid_ == OB_DEVICE_MAX_PRO_PID || deviceInfo_->pid_ == OB_DEVICE_GEMINI_UW_PID) {
        static const std::vector<OBMultiDeviceSyncMode> supportedSyncModes = {
            OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN,
            OB_MULTI_DEVICE_SYNC_MODE_PRIMARY,
            OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED,
        };
        auto deviceSyncConfigurator = std::make_shared<OpenNIDeviceSyncConfigurator>(this, supportedSyncModes);
        registerComponent(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, deviceSyncConfigurator);
    }
}

void MaxDevice::initSensorList() {
    OpenNIDeviceBase::initSensorList();

    registerComponent(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER, [this]() { return std::make_shared<OpenNIStreamProfileFilter>(this); });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto depthPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_DEPTH
               && (std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == OB_DEVICE_MAX_PRO_PID
                   || std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == OB_DEVICE_DABAI_MAX_PID
                   || std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == OB_DEVICE_GEMINI_UW_PID);
    });

    if(depthPortInfoIter != sourcePortInfoList.end()) {
        auto depthPortInfo = *depthPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_DEPTH_SENSOR,
            [this, depthPortInfo]() {
                auto port   = getSourcePort(depthPortInfo);
                auto sensor = std::make_shared<MaxDisparitySensor>(this, OB_SENSOR_DEPTH, port);
                sensor->enableTimestampAnomalyDetection(false);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                auto propServer = getPropertyServer();
                propServer->setPropertyValueT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, true);

                propServer->setPropertyValueT(OB_PROP_DEPTH_PRECISION_LEVEL_INT, OB_PRECISION_1MM);
                sensor->setDepthUnit(1.0f);

                sensor->markOutputDisparityFrame(true);
                sensor->initProfileVirtualRealMap();

                initSensorStreamProfile(sensor);

                auto streamProfileFilter = getComponentT<IStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
                sensor->setStreamProfileFilter(streamProfileFilter.get());

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
            auto port     = getSourcePort(depthPortInfo);
            auto accessor = std::make_shared<VendorPropertyAccessor>(this, port);
            return accessor;
        });

        // The device monitor is using the depth port(uvc xu)
        registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, depthPortInfo]() {
            auto port       = getSourcePort(depthPortInfo);
            auto devMonitor = std::make_shared<DeviceMonitor>(this, port);
            return devMonitor;
        });
    }

    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_COLOR
               && (std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == MAX_PRO_COLOR_PID
                   || std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == GEMINI_UW_COLOR_PID);
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
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_NV21, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGR, OB_FORMAT_ANY, nullptr },
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_BGRA, OB_FORMAT_ANY, nullptr },
                };

                auto formatConverter = getSensorFrameFilter("FormatConverter", OB_SENSOR_COLOR, false);
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                sensor->enableTimestampAnomalyDetection(false);
                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                auto streamProfileFilter = getComponentT<IStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
                sensor->setStreamProfileFilter(streamProfileFilter.get());

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

}

void MaxDevice::initProperties() {
    OpenNIDeviceBase::initProperties();

    auto propertyServer = getPropertyServer();
    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_COLOR) {
            auto uvcPropertyAccessor = std::make_shared<LazyPropertyAccessor>([this, &sourcePortInfo]() {
                auto port     = getSourcePort(sourcePortInfo);
                auto accessor = std::make_shared<UvcPropertyAccessor>(port);
                return accessor;
            });

            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAIN_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAMMA_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
        }

        if(sensor == OB_SENSOR_DEPTH) {
            propertyServer->registerProperty(OB_PROP_IR_CHANNEL_DATA_SOURCE_INT, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_IR_LONG_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_DEPTH_LOAD_ENGINE_GROUP_PARAM_INT, "", "w", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_TEMPERATURE_COMPENSATION_BOOL, "rw", "rw", vendorPropertyAccessor_);
            auto deviceTempPropertyAccessor = std::make_shared<OpenNITemperatureStructurePropertyAccessor>(this);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_TEMPERATURE, "r", "r", deviceTempPropertyAccessor);
            if(deviceInfo_->pid_ == OB_DEVICE_DABAI_MAX_PID) {
                propertyServer->registerProperty(OB_PROP_LDP_STATUS_BOOL, "r", "r", vendorPropertyAccessor_);
                propertyServer->registerProperty(OB_PROP_LDP_BOOL, "rw", "rw", vendorPropertyAccessor_);
            }
            if(deviceInfo_->pid_ == OB_DEVICE_MAX_PRO_PID || deviceInfo_->pid_ == OB_DEVICE_GEMINI_UW_PID) {
                propertyServer->registerProperty(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, "rw", "rw", vendorPropertyAccessor_);
            }
            auto heartBeatPropertyAccessor = std::make_shared<OpenNIHeartBeatPropertyAccessor>(this);
            propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "rw", "rw", heartBeatPropertyAccessor);
        }
    }
}

}  // namespace libobsensor
