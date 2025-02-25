// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDevice.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/imu/ImuStreamer.hpp"
#include "sensor/imu/AccelSensor.hpp"
#include "sensor/imu/GyroSensor.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

#include "metadata/FrameMetadataParserContainer.hpp"
#include "timestamp/GlobalTimestampFitter.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"
#include "timestamp/DeviceClockSynchronizer.hpp"
#include "property/LiDARPropertyAccessor.hpp"
#include "property/UvcPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "property/PrivateFilterPropertyAccessors.hpp"
#include "param/AlgParamManager.hpp"
#include "syncconfig/DeviceSyncConfigurator.hpp"

#include "FilterFactory.hpp"
#include "publicfilters/FormatConverterProcess.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"

#if defined(BUILD_NET_PAL)
// TODO #include "ethernet/RTSPStreamPort.hpp"
#endif

namespace libobsensor {

LiDARDevice::LiDARDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

LiDARDevice::~LiDARDevice() noexcept {}

void LiDARDevice::init() {
    initProperties();
    initSensorList();

    fetchDeviceInfo();
}

void LiDARDevice::fetchDeviceInfo() {
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
        auto                        propertyServer = getPropertyServer();
        const std::vector<uint8_t> &data           = propertyServer->getStructureData(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, PROP_ACCESS_INTERNAL);
        if(data.empty()) {
            deviceInfo_->fwVersion_ = "unknown";  // TODO getFirmwareVersion
        }
        else {
            deviceInfo_->fwVersion_ = std::string(data.begin(), data.end());
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("fetch LiDAR deviceInfo error!"); })

    deviceInfo_->deviceSn_            = enumInfo_->getDeviceSn();
    deviceInfo_->asicName_            = "unknown";  // TODO
    deviceInfo_->hwVersion_           = "unknown";  // TODO lidar doesn't have hw version
    deviceInfo_->type_                = 0xFFFF;     // TODO lidar doesn't have type
    deviceInfo_->supportedSdkVersion_ = "2.2.8";    // TODO fix

    deviceInfo_->fullName_ = enumInfo_->getFullName();
}

void LiDARDevice::initSensorList() {
    // TODO
}

void LiDARDevice::initProperties() {
    const auto &sourcePortInfoList = enumInfo_->getSourcePortInfoList();
    auto        vendorPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(),
                                           [](const std::shared_ptr<const SourcePortInfo> &portInfo) { return portInfo->portType == SOURCE_PORT_NET_VENDOR; });

    if(vendorPortInfoIter == sourcePortInfoList.end()) {
        return;
    }

    auto propertyServer = std::make_shared<PropertyServer>(this);
    auto vendorPortInfo = *vendorPortInfoIter;

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

    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_IP_ADDRESS, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_PORT_INT, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MAC_ADDRESS, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_SUBNET_MASK, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_SPEED_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_DIRECTION_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TRANSFER_PROTOCOL_INT, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_WORK_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_SERIAL_NUMBER, "r", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_ECHO_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APPLY_CONFIGS_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FOV_SIZE_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FRENQUENCY_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FOV_FACTOR_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_ON_OFF_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_RESTART_MEMS_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SAVE_MEMS_PARAM_INT, "w", "w", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_PRODUCT_MODEL, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FPGA_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_LIDAR_STATUS_INFO, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_WARNING_INFO_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MCU_TEMPERATURE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_FPGA_TEMPERATURE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MOTOR_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APD_HIGH_VOLTAGE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APD_TEMPERATURE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TX_HIGH_POWER_VOLTAGE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TX_LOWER_POWER_VOLTAGE_FLOAT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MEMS_VERSION, "r", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_PROP_LIDAR_INITIATE_DEVICE_CONNECTION_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, "", "w", vendorPropertyAccessor);

    // register property server
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    // connect
    BEGIN_TRY_EXECUTE({ propertyServer->setPropertyValueT(OB_PROP_LIDAR_INITIATE_DEVICE_CONNECTION_INT, 0x12345678); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Connect LiDAR device error!"); })
}

void LiDARDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    // TODO
}

}  // namespace libobsensor
