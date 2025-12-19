// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDevice.hpp"
#include "environment/EnvConfig.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "sensor/lidar/LiDARStreamer.hpp"
#include "sensor/lidar/LiDARSensor.hpp"
#include "sensor/lidar/LiDARImuStreamer.hpp"
#include "sensor/lidar/LiDARAccelSensor.hpp"
#include "sensor/lidar/LiDARGyroSensor.hpp"
#include "property/LiDARPropertyAccessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/FilterPropertyAccessors.hpp"
#include "LiDARStreamProfileFilter.hpp"
#include "utils/BufferParser.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "monitor/LiDARDeviceMonitor.hpp"
#include "FilterFactory.hpp"
#include "LiDARAlgParamManager.hpp"
#include <sstream>
#include <iomanip>

#if defined(BUILD_NET_PAL)
#include "ethernet/LiDARDataStreamPort.hpp"
#endif

namespace libobsensor {

LiDARDevice::LiDARDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : LiDARDeviceBase(info) {
    init();
}

LiDARDevice::~LiDARDevice() noexcept {}

void LiDARDevice::init() {
    initProperties();
    initSensorList();

    fetchDeviceInfo();

    registerComponent(
    OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER,
    [this]() {
        std::shared_ptr<DeviceActivityRecorder> activityRecorder;
        TRY_EXECUTE({ activityRecorder = std::make_shared<DeviceActivityRecorder>(this); })
        return activityRecorder;
    },
    false);

    auto algParamManager = std::make_shared<LiDARAlgParamManager>(this);
    registerComponent(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, algParamManager);
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
        auto propertyServer = getPropertyServer();

        // firmware version
        auto data               = propertyServer->getStructureData(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, PROP_ACCESS_INTERNAL);
        deviceInfo_->fwVersion_ = Uint8toString(data, "unknown");
        // sn
        data                   = propertyServer->getStructureData(OB_STRUCT_DEVICE_SERIAL_NUMBER, PROP_ACCESS_INTERNAL);
        deviceInfo_->deviceSn_ = Uint8toString(data, enumInfo_->getDeviceSn());
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("fetch LiDAR deviceInfo error!");
        deviceInfo_->fwVersion_ = "unknown";
        deviceInfo_->deviceSn_  = enumInfo_->getDeviceSn();
        throw;
    })

    deviceInfo_->asicName_            = "unknown";  // TODO
    deviceInfo_->hwVersion_           = "unknown";  // TODO LiDAR doesn't have hw version
    deviceInfo_->type_                = 0xFFFF;     // TODO LiDAR doesn't have type
    deviceInfo_->supportedSdkVersion_ = "2.2.10";   // TODO fix

    deviceInfo_->fullName_ = enumInfo_->getFullName();

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";
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
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_SUBNET_MASK, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APPLY_CONFIGS_INT, "w", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FOV_SIZE_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FRENQUENCY_FLOAT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_REPETITIVE_SCAN_MODE_INT, "rw", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_PRODUCT_MODEL, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_FPGA_VERSION, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_WARNING_INFO_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MCU_TEMPERATURE_INT, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APD_TEMPERATURE_INT, "r", "r", vendorPropertyAccessor);

    propertyServer->registerProperty(OB_STRUCT_DEVICE_SERIAL_NUMBER, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_SPEED_INT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_INITIATE_DEVICE_CONNECTION_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_LIDAR_IMU_FULL_SCALE_RANGE, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_SCAN_DIRECTION_INT, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TRANSFER_PROTOCOL_INT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_FOV_FACTOR_FLOAT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_MEMS_ON_OFF_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_RESTART_MEMS_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_LIDAR_STATUS_INFO, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_FPGA_TEMPERATURE_INT, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MOTOR_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TX_HIGH_POWER_VOLTAGE_INT, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_TX_LOWER_POWER_VOLTAGE_INT, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_LIDAR_MEMS_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_APD_HIGH_VOLTAGE_INT, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_IMU_STREAM_PORT_INT, "", "w", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_LIDAR_IMU_FRAME_RATE_INT, "", "rw", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_RAW_DATA_IMU_CALIB_PARAM, "", "r", vendorPropertyAccessor);
    

    // register property server
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    // register property server
    propertyServer->registerAccessCallback(
        OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, [&](uint32_t, const uint8_t *data, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                if(lidarFilterList_["LiDARPointFilter"]) {
                    lidarFilterList_["LiDARPointFilter"]->setConfigValue("FilterLevel", static_cast<double>(static_cast<int>(*data)));
                }
            }
        });

    // set work mode
    /*
    // TODO: The work mode setting and acquisition are not open temporarily.
    BEGIN_TRY_EXECUTE({
        // set to normal work mode
        propertyServer->setPropertyValueT(OB_PROP_LIDAR_WORK_MODE_INT, 0);
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_ERROR("Set LiDAR device work mode to normal error!"); })
    */
}

void LiDARDevice::initSensorList() {
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

            auto filterFactory = FilterFactory::getInstance();

            // create filters
            lidarFilterList_.clear();
            std::vector<std::pair<std::string, std::shared_ptr<IFilter>>> sortFilters;

            std::vector<std::string> filterNames = { "LiDARPointFilter", "LiDARFormatConverter" };
            for(const auto &filterName: filterNames) {
                lidarFilterList_[filterName] = filterFactory->createFilter(filterName);
                sortFilters.push_back({ filterName, lidarFilterList_[filterName] });
            }

            auto streamer = std::make_shared<LiDARStreamer>(this, dataStreamPort, sortFilters);

            return streamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_LIDAR_SENSOR,
            [this, lidarPortInfo]() {
                auto port              = getSourcePort(lidarPortInfo);
                auto streamer          = getComponentT<LiDARStreamer>(OB_DEV_COMPONENT_LIDAR_STREAMER);
                auto streamerSharedPtr = streamer.get();
                auto sensor            = std::make_shared<LiDARSensor>(this, port, streamerSharedPtr);

                sensor->enableTimestampAnomalyDetection(false);
                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_LIDAR, lidarPortInfo);
    }

    // imu sensor
    auto imuPortInfoIter = std::find_if(sourcePortInfoList.begin(), sourcePortInfoList.end(), [](const std::shared_ptr<const SourcePortInfo> &portInfo) {
        if(portInfo->portType != SOURCE_PORT_NET_LIDAR_VENDOR_STREAM) {
            return false;
        }
        return (std::dynamic_pointer_cast<const LiDARDataStreamPortInfo>(portInfo)->streamType == OB_STREAM_ACCEL);
    });
    if(imuPortInfoIter != sourcePortInfoList.end()) {
        auto imuPortInfo = *imuPortInfoIter;

        registerComponent(OB_DEV_COMPONENT_IMU_STREAMER, [this, imuPortInfo]() {
            // the gyro and accel are both on the same port and share the same filter
            auto port           = getSourcePort(imuPortInfo);
            auto dataStreamPort = std::dynamic_pointer_cast<IDataStreamPort>(port);
            auto imuStreamer    = std::make_shared<LiDARImuStreamer>(this, dataStreamPort);
            return imuStreamer;
        });

        registerComponent(
            OB_DEV_COMPONENT_ACCEL_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<LiDARImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<LiDARAccelSensor>(this, port, imuStreamerSharedPtr);

                sensor->enableTimestampAnomalyDetection(false);
                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_ACCEL, imuPortInfo);

        registerComponent(
            OB_DEV_COMPONENT_GYRO_SENSOR,
            [this, imuPortInfo]() {
                auto port                 = getSourcePort(imuPortInfo);
                auto imuStreamer          = getComponentT<LiDARImuStreamer>(OB_DEV_COMPONENT_IMU_STREAMER);
                auto imuStreamerSharedPtr = imuStreamer.get();
                auto sensor               = std::make_shared<LiDARGyroSensor>(this, port, imuStreamerSharedPtr);

                sensor->enableTimestampAnomalyDetection(false);
                initSensorStreamProfile(sensor);
                return sensor;
            },
            true);
        registerSensorPortInfo(OB_SENSOR_GYRO, imuPortInfo);
    }
}

void LiDARDevice::initSensorStreamProfile(std::shared_ptr<ISensor> sensor) {
    auto         sensorType = sensor->getSensorType();
    OBStreamType streamType = utils::mapSensorTypeToStreamType(sensorType);

    // set stream profile filter
    auto streamProfileFilter = getComponentT<IStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
    sensor->setStreamProfileFilter(streamProfileFilter.get());

    // hardcoded here
    if(streamType == OB_STREAM_LIDAR) {
        const std::vector<OBLiDARScanRate> scanRates = { OB_LIDAR_SCAN_20HZ };
        const std::vector<OBFormat>        formats   = { OB_FORMAT_LIDAR_SPHERE_POINT, OB_FORMAT_LIDAR_POINT, OB_FORMAT_LIDAR_CALIBRATION };
        StreamProfileList                  profileList;

        for(auto rate: scanRates) {
            for(auto format: formats) {
                auto profile = StreamProfileFactory::createLiDARStreamProfile(rate, format);
                profileList.push_back(profile);
            }
        }
        if(profileList.size()) {
            auto defaultProfile = profileList[0];
            BEGIN_TRY_EXECUTE({
                auto propertyServer   = getPropertyServer();
                auto currentScanSpeed = propertyServer->getPropertyValueT<int32_t>(OB_PROP_LIDAR_SCAN_SPEED_INT);
                auto it               = std::find_if(profileList.begin(), profileList.end(), [&](const std::shared_ptr<const StreamProfile> &profile) {
                    return profile->as<LiDARStreamProfile>()->getInfo().scanSpeed == currentScanSpeed;
                });
                if(it != profileList.end()) {
                    defaultProfile = *it;
                }
            })
            CATCH_EXCEPTION_AND_EXECUTE({
                LOG_ERROR("fetch LiDAR scan speed error!");
            })
            sensor->setStreamProfileList(profileList);
            sensor->updateDefaultStreamProfile(defaultProfile);

            auto algParamManager = getComponentT<LiDARAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
            algParamManager->bindExtrinsic(profileList);
        }
    }
    else if(streamType == OB_STREAM_ACCEL) {
        // LiDAR imu accel
        const std::vector<OBAccelSampleRate> rates = { OB_SAMPLE_RATE_25_HZ, OB_SAMPLE_RATE_50_HZ, OB_SAMPLE_RATE_100_HZ, OB_SAMPLE_RATE_200_HZ };
        StreamProfileList                    profileList;
        auto                                 lazySensor = std::make_shared<LazySensor>(this, OB_SENSOR_ACCEL);
        OBAccelFullScaleRange                accelRange;
        OBGyroFullScaleRange                 gyroRange;

        getImuFullScaleRange(accelRange, gyroRange);
        for(auto rate: rates) {
            auto profile = StreamProfileFactory::createAccelStreamProfile(lazySensor, accelRange, rate);
            profileList.push_back(profile);
        }
        if(profileList.size()) {
            sensor->setStreamProfileList(profileList);
            sensor->updateDefaultStreamProfile(profileList[1]);

            auto algParamManager = getComponentT<LiDARAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
            algParamManager->bindStreamProfileParams(profileList);
        }
    }
    else if(streamType == OB_STREAM_GYRO) {
        // LiDAR imu accel
        const std::vector<OBGyroSampleRate> rates = { OB_SAMPLE_RATE_25_HZ, OB_SAMPLE_RATE_50_HZ, OB_SAMPLE_RATE_100_HZ, OB_SAMPLE_RATE_200_HZ };
        StreamProfileList                   profileList;
        auto                                lazySensor = std::make_shared<LazySensor>(this, OB_SENSOR_GYRO);
        OBAccelFullScaleRange               accelRange;
        OBGyroFullScaleRange                gyroRange;

        getImuFullScaleRange(accelRange, gyroRange);
        for(auto rate: rates) {
            auto profile = StreamProfileFactory::createGyroStreamProfile(lazySensor, gyroRange, rate);
            profileList.push_back(profile);
        }
        if(profileList.size()) {
            sensor->setStreamProfileList(profileList);
            sensor->updateDefaultStreamProfile(profileList[1]);
            auto algParamManager = getComponentT<LiDARAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER);
            algParamManager->bindStreamProfileParams(profileList);
        }
    }
}

void LiDARDevice::getImuFullScaleRange(OBAccelFullScaleRange &accel, OBGyroFullScaleRange &gyro) {
    auto propertyServer = getPropertyServer();
    auto data           = propertyServer->getStructureData(OB_STRUCT_LIDAR_IMU_FULL_SCALE_RANGE, PROP_ACCESS_INTERNAL);

    if(data.size() != sizeof(uint32_t)) {
        throw invalid_value_exception("OB_STRUCT_LIDAR_IMU_FULL_SCALE_RANGE response with wrong data size");
    }
    auto     value      = data.data();
    uint16_t accelValue = (((uint16_t)value[0]) << 8) | value[1];
    switch(accelValue) {
    case 2:
        accel = OB_ACCEL_FS_2g;
        break;
    case 3:
        accel = OB_ACCEL_FS_3g;
        break;
    case 4:
        accel = OB_ACCEL_FS_4g;
        break;
    case 6:
        accel = OB_ACCEL_FS_6g;
        break;
    case 8:
        accel = OB_ACCEL_FS_8g;
        break;
    case 12:
        accel = OB_ACCEL_FS_12g;
        break;
    case 16:
        accel = OB_ACCEL_FS_16g;
        break;
    case 24:
        accel = OB_ACCEL_FS_24g;
        break;
    default:
        throw invalid_value_exception("OB_STRUCT_LIDAR_IMU_FULL_SCALE_RANGE response with invalid accel full scale range");
        break;
    }

    uint16_t gyroValue = (((uint16_t)value[2]) << 8) | value[3];
    switch(gyroValue) {
    case 16:
        gyro = OB_GYRO_FS_16dps;
        break;
    case 31:
        gyro = OB_GYRO_FS_31dps;
        break;
    case 62:
        gyro = OB_GYRO_FS_62dps;
        break;
    case 125:
        gyro = OB_GYRO_FS_125dps;
        break;
    case 250:
        gyro = OB_GYRO_FS_250dps;
        break;
    case 400:
        gyro = OB_GYRO_FS_400dps;
        break;
    case 500:
        gyro = OB_GYRO_FS_500dps;
        break;
    case 800:
        gyro = OB_GYRO_FS_800dps;
        break;
    case 1000:
        gyro = OB_GYRO_FS_1000dps;
        break;
    case 2000:
        gyro = OB_GYRO_FS_2000dps;
        break;
    default:
        throw invalid_value_exception("OB_STRUCT_LIDAR_IMU_FULL_SCALE_RANGE response with invalid gyro full scale range");
        break;
    }
}

}  // namespace libobsensor
