// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIDeviceBase.hpp"
#include "DevicePids.hpp"
#include "InternalTypes.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "FilterFactory.hpp"
#include "OpenNIPropertyAccessors.hpp"
#include "OpenNIAlgParamManager.hpp"
#include "OpenNIFrameTimestampCalculator.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "property/PrivateFilterPropertyAccessors.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "firmwareupdater/firmwareupdateguard/FirmwareUpdateGuards.hpp"
#include <algorithm>


namespace libobsensor {

constexpr uint8_t INTERFACE_IR    = 2;

OpenNIDeviceBase::OpenNIDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
}

OpenNIDeviceBase::~OpenNIDeviceBase() noexcept {

}

void OpenNIDeviceBase::init() {
    initSensorList();
    initProperties();
    fetchDeviceInfo();
    fetchExtensionInfo();

    videoFrameTimestampCalculatorCreator_ = [this]() { return std::make_shared<OpenNIFrameTimestampCalculator>(this, deviceTimeFreq_, frameTimeFreq_); };

    auto algParamManager = std::make_shared<OpenNIAlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);

    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

void OpenNIDeviceBase::initSensorList() {
    registerComponent(OB_DEV_COMPONENT_FRAME_PROCESSOR_FACTORY, [this]() {
        std::shared_ptr<FrameProcessorFactory> factory;
        TRY_EXECUTE({ factory = std::make_shared<FrameProcessorFactory>(this); })
        return factory;
    });

    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto irPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        return portInfo->portType == SOURCE_PORT_USB_UVC && std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->infIndex == INTERFACE_IR;
    });

    if(irPortInfoIter != sourcePortInfoList.end()) {
        auto irPortInfo = *irPortInfoIter;
        registerComponent(
            OB_DEV_COMPONENT_IR_SENSOR,
            [this, irPortInfo]() {
                auto port   = getSourcePort(irPortInfo);
                auto sensor = std::make_shared<VideoSensor>(this, OB_SENSOR_IR, port);

                std::vector<FormatFilterConfig> formatFilterConfigs = {
                    { FormatFilterPolicy::REMOVE, OB_FORMAT_Z16, OB_FORMAT_ANY, nullptr },
                };
                sensor->updateFormatFilterConfig(formatFilterConfigs);

                auto frameTimestampCalculator = videoFrameTimestampCalculatorCreator_();
                sensor->setFrameTimestampCalculator(frameTimestampCalculator);

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
}

void OpenNIDeviceBase::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto streamProfile = StreamProfileFactory::getDefaultStreamProfileFromEnvConfig(deviceInfo_->name_, sensor->getSensorType());
    if(streamProfile) {
        sensor->updateDefaultStreamProfile(streamProfile);
    }

    // bind params: extrinsics, intrinsics, etc.
    auto profiles = sensor->getStreamProfileList();
    {
        auto algParamManager = getComponentT<OpenNIAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
        algParamManager->bindStreamProfileParams(profiles);
    }

    auto sensorType = sensor->getSensorType();
    LOG_INFO("Sensor {} created! Found {} stream profiles.", sensorType, profiles.size());
    for(auto &profile: profiles) {
        LOG_INFO(" - {}", profile);
    }
}

void OpenNIDeviceBase::initProperties() {
    auto propertyServer = std::make_shared<PropertyServer>(this);

    auto d2dPropertyAccessor = std::make_shared<OpenNIDisp2DepthPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, "", "rw", d2dPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_PRECISION_LEVEL_INT, "rw", "rw", d2dPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST, "r", "r", d2dPropertyAccessor);

    auto privatePropertyAccessor = std::make_shared<PrivateFilterPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, "rw", "rw", privatePropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, "rw", "rw", privatePropertyAccessor);

    auto frameTransformPropertyAccessor = std::make_shared<OpenNIFrameTransformPropertyAccessor>(this);
    propertyServer->registerProperty(OB_PROP_DEPTH_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_DEPTH_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IR_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    if(isComponentExists(OB_DEV_COMPONENT_COLOR_SENSOR)) {
        propertyServer->registerProperty(OB_PROP_COLOR_MIRROR_BOOL, "rw", "rw", frameTransformPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_FLIP_BOOL, "rw", "rw", frameTransformPropertyAccessor);
        propertyServer->registerProperty(OB_PROP_COLOR_ROTATE_INT, "rw", "rw", frameTransformPropertyAccessor);
    }

    auto sensors = getSensorTypeList();
    for(auto &sensor: sensors) {
        auto &sourcePortInfo = getSensorPortInfo(sensor);
        if(sensor == OB_SENSOR_DEPTH) {
            vendorPropertyAccessor_ = std::make_shared<LazySuperPropertyAccessor>([this, &sourcePortInfo]() {
                auto port                   = getSourcePort(sourcePortInfo);
                auto vendorPropertyAccessor = std::make_shared<VendorPropertyAccessor>(this, port);
                return vendorPropertyAccessor;
            });

            propertyServer->registerProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_DEPTH_EXPOSURE_INT, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_DEPTH_GAIN_INT, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_LASER_BOOL, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_DEPTH_HOLEFILTER_BOOL, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, "rw", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_SDK_DEPTH_FRAME_UNPACK_BOOL, "", "rw", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_SDK_IR_FRAME_UNPACK_BOOL, "", "rw", vendorPropertyAccessor_); 
            propertyServer->registerProperty(OB_PROP_DEVICE_RESET_BOOL, "", "w", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_STOP_DEPTH_STREAM_BOOL, "", "w", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_STOP_IR_STREAM_BOOL, "", "w", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_PROP_STOP_COLOR_STREAM_BOOL, "", "w", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_RAW_DATA_DUAL_CAMERA_PARAMS_0, "", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_RAW_DATA_DUAL_CAMERA_PARAMS_1, "", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_RAW_DATA_DUAL_CAMERA_PARAMS_2, "", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_STRUCT_EXTENSION_PARAM, "", "r", vendorPropertyAccessor_);
            propertyServer->registerProperty(OB_STRUCT_INTERNAL_CAMERA_PARAM, "", "r", vendorPropertyAccessor_);
        }
    }

    propertyServer->aliasProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
    propertyServer->aliasProperty(OB_PROP_IR_EXPOSURE_INT, OB_PROP_DEPTH_EXPOSURE_INT);
    propertyServer->aliasProperty(OB_PROP_IR_GAIN_INT, OB_PROP_DEPTH_GAIN_INT);

    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

std::vector<std::shared_ptr<IFilter>> OpenNIDeviceBase::createRecommendedPostProcessingFilters(OBSensorType type) {
    if(type != OB_SENSOR_DEPTH) {
        return {};
    }
    auto                                  filterFactory = FilterFactory::getInstance();
    std::vector<std::shared_ptr<IFilter>> depthFilterList;

    if(filterFactory->isFilterCreatorExists("EdgeNoiseRemovalFilter")) {
        auto enrFilter = filterFactory->createFilter("EdgeNoiseRemovalFilter");
        enrFilter->enable(false);
        // todo: set default values
        depthFilterList.push_back(enrFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        spatFilter->enable(false);
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "64", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        tempFilter->enable(false);
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
        hfFilter->enable(false);
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }

    return depthFilterList;
}

}  // namespace libobsensor
