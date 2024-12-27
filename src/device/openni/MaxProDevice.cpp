// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MaxProDevice.hpp"
#include "DevicePids.hpp"
#include "InternalTypes.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "FilterFactory.hpp"
#include "OpenNIPropertyAccessors.hpp"
#include "OpenNIAlgParamManager.hpp"
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
#include <algorithm>


namespace libobsensor {

constexpr uint8_t  INTERFACE_COLOR   = 0;
constexpr uint16_t MAX_PRO_COLOR_PID = 0x0560;

MaxProDevice::MaxProDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : OpenNIDeviceBase(info) {
    init();
}

MaxProDevice::~MaxProDevice() noexcept {

}

void MaxProDevice::init() {
    OpenNIDeviceBase::init();


}

void MaxProDevice::initSensorList() {
    OpenNIDeviceBase::initSensorList();

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto colorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_COLOR
               && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->pid == MAX_PRO_COLOR_PID;
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
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_MJPG, OB_FORMAT_ANY, nullptr },
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

                /*auto colorMdParserContainer = getComponentT<IFrameMetadataParserContainer>(OB_DEV_COMPONENT_COLOR_FRAME_METADATA_CONTAINER);
                sensor->setFrameMetadataParserContainer(colorMdParserContainer.get());*/

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

                /*auto globalFrameTimestampCalculator = std::make_shared<GlobalTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_);
                sensor->setGlobalTimestampCalculator(globalFrameTimestampCalculator);*/

                auto frameProcessor = getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);
                if(frameProcessor) {
                    sensor->setFrameProcessor(frameProcessor.get());
                }

                //initSensorStreamProfile(sensor);

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

void MaxProDevice::initProperties() {
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
            propertyServer->registerProperty(OB_PROP_COLOR_SATURATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_EXPOSURE_INT, "rw", "rw", uvcPropertyAccessor);
            //propertyServer->registerProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, "rw", "rw", uvcPropertyAccessor);
            //propertyServer->registerProperty(OB_PROP_COLOR_WHITE_BALANCE_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BRIGHTNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_SHARPNESS_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_CONTRAST_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_GAMMA_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, "rw", "rw", uvcPropertyAccessor);
            propertyServer->registerProperty(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, "rw", "rw", uvcPropertyAccessor);
        }

        if(sensor == OB_SENSOR_DEPTH) {
            propertyServer->registerProperty(OB_PROP_IR_CHANNEL_DATA_SOURCE_INT, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_IR_LONG_EXPOSURE_BOOL, "w", "w", vendorPropertyAccessor_);
            // propertyServer->registerProperty(OB_PROP_WATCHDOG_BOOL, "w", "w", vendorPropertyAccessor_);
            // propertyServer->registerProperty(OB_PROP_HEARTBEAT_BOOL, "w", "w", vendorPropertyAccessor_);
            // propertyServer->registerProperty(OB_STRUCT_CUSTOMER_DATA, "w", "w", vendorPropertyAccessor_);
        }
    }


}

}  // namespace libobsensor
