// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MS600Device.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/lidar/MS600Streamer.hpp"
#include "sensor/lidar/LiDARSensor.hpp"
#include "property/LiDARPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "LiDARStreamProfileFilter.hpp"
#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "monitor/LiDARDeviceMonitor.hpp"
#include <sstream>
#include <iomanip>

#if defined(BUILD_NET_PAL)
#include "ethernet/LiDARDataStreamPort.hpp"
#endif

namespace libobsensor {

MS600Device::MS600Device(const std::shared_ptr<const IDeviceEnumInfo> &info) : LiDARDeviceBase(info) {
    init();
}

MS600Device::~MS600Device() noexcept {}

void MS600Device::init() {
    initProperties();
    initSensorList();

    fetchDeviceInfo();
}

void MS600Device::fetchDeviceInfo() {
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

    BEGIN_TRY_EXECUTE({
        auto propertyServer = getPropertyServer();

        // firmware version
        auto data               = propertyServer->getStructureData(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, PROP_ACCESS_INTERNAL);
        deviceInfo_->fwVersion_ = Uint8toString(data, "unknown");
        // sn
        data                   = propertyServer->getStructureData(OB_RAW_DATA_LIDAR_SERIAL_NUMBER, PROP_ACCESS_INTERNAL);
        deviceInfo_->deviceSn_ = Uint8toString(data, enumInfo_->getDeviceSn());
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("fetch LiDAR deviceInfo error!");
        deviceInfo_->fwVersion_ = "unknown";
        deviceInfo_->deviceSn_  = enumInfo_->getDeviceSn();
    })

    deviceInfo_->asicName_            = "unknown";  // TODO
    deviceInfo_->hwVersion_           = "unknown";  // TODO LiDAR doesn't have hw version
    deviceInfo_->type_                = 0xFFFF;     // TODO LiDAR doesn't have type
    deviceInfo_->supportedSdkVersion_ = "2.2.10";   // TODO fix

    deviceInfo_->fullName_ = enumInfo_->getFullName();

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
}

void MS600Device::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                                  [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_NET_VENDOR; });

    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto propertyServer = std::make_shared<PropertyServer>(this);
    auto vendorPortInfo = *vendorPortInfoIter;

    // The device monitor
    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this, vendorPortInfo]() {
        auto port       = getSourcePort(vendorPortInfo);
        auto devMonitor = std::make_shared<LiDARDeviceMonitor>(this, port);
        return devMonitor;
    });

    // common property accessor
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<LiDARPropertyAccessor>(this, port);
        return accessor;
    });

    // the main property accessor
    registerComponent(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR, [this, vendorPortInfo]() {
        auto port     = getSourcePort(vendorPortInfo);
        auto accessor = std::make_shared<LiDARPropertyAccessor>(this, port);
        return accessor;
    });

    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_IP_ADDRESS, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_PORT_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MAC_ADDRESS, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_SPEED_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_DIRECTION_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TRANSFER_PROTOCOL_INT, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SPECIFIC_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APPLY_CONFIGS_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_PRODUCT_MODEL, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FPGA_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_WARNING_INFO_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MCU_TEMPERATURE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_FPGA_TEMPERATURE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MOTOR_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APD_HIGH_VOLTAGE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TX_HIGH_POWER_VOLTAGE_FLOAT, "r", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_LIDAR_INITIATE_DEVICE_CONNECTION_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_WORK_MODE_INT, "rw", "rw", vendorPropertyAccessor);

    // register property server
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);
}

void MS600Device::initSensorList() {
    // stream filter
    registerComponent(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER, [this]() { return std::make_shared<LiDARStreamProfileFilter>(this); });

    // sensor
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto lidarPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        if(portInfo->portType != SOURCE_PORT_NET_LIDAR_VENDOR_STREAM) {
            return false;
        }
        return (std::dynamic_pointer_cast<const LiDARDataStreamPortInfo>(portInfo)->streamType == OB_STREAM_LIDAR);
    });

    // LiDAR sensor
    if(lidarPortInfoIter != sourcePortInfoList.end()) {
        // found LiDAR port
        auto lidarPortInfo = *lidarPortInfoIter;
        registerComponent(OB_DEV_COMPONENT_LIDAR_STREAMER, [this, lidarPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter
            auto port           = getSourcePort(lidarPortInfo);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto streamer       = std::make_shared<MS600Streamer>(this, dataStreamPort);
            return streamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_LIDAR_SENSOR,
            [this, lidarPortInfo]() {
                auto port              = getSourcePort(lidarPortInfo);
                auto streamer          = getComponentT<MS600Streamer>(OB_DEV_COMPONENT_LIDAR_STREAMER);
                auto streamerSharedPtr = streamer.get();
                auto sensor            = std::make_shared<LiDARSensor>(this, port, streamerSharedPtr);

                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_LIDAR, lidarPortInfo);
    }
}

void MS600Device::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto         sensorType = sensor->getSensorType();
    OBStreamType streamType = utils::mapSensorTypeToStreamType(sensorType);

    // set stream profile filter
    auto streamProfileFilter = getComponentT<IStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
    sensor->setStreamProfileFilter(streamProfileFilter.get());

    // TODO hardcoded here
    if(streamType == OB_STREAM_LIDAR) {
        // LiDAR
        const std::vector<OBLiDARScanRate> scanRates = { OB_LIDAR_SCAN_40HZ, OB_LIDAR_SCAN_30HZ, OB_LIDAR_SCAN_25HZ, OB_LIDAR_SCAN_20HZ, OB_LIDAR_SCAN_15HZ };
        const std::vector<OBFormat>        formats   = { OB_FORMAT_LIDAR_SCAN };
        StreamProfileList                  profileList;

        for(auto rate: scanRates) {
            for(auto format: formats) {
                auto profile = StreamProfileFactory::createLiDARStreamProfile(rate, format);
                profileList.push_back(profile);
            }
        }
        if(profileList.size()) {
            sensor->setStreamProfileList(profileList);
            sensor->updateDefaultStreamProfile(profileList[0]);
        }
    }
}

}  // namespace libobsensor
