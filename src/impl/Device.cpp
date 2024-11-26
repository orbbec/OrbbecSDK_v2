// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Device.h"

#include "ImplTypes.hpp"
#include "exception/ObException.hpp"

#include "IDeviceManager.hpp"
#include "IProperty.hpp"
#include "IDevice.hpp"
#include "IDeviceMonitor.hpp"
#include "IAlgParamManager.hpp"
#include "IFrameTimestamp.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void ob_delete_device_list(ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    delete list;
}
HANDLE_EXCEPTIONS_NO_RETURN(list)

uint32_t ob_device_list_get_count(const ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    return static_cast<uint32_t>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

const char *ob_device_list_get_device_name(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getFullName().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

int ob_device_list_get_device_pid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getPid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

int ob_device_list_get_device_vid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getVid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

const char *ob_device_list_get_device_uid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getUid().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_serial_number(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getDeviceSn().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_connection_type(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getConnectionType().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_ip_address(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("get ipAddress() failed! Only valid for Ethernet devices.");
        return "0.0.0.0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->address.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_device *ob_device_list_get_device(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info      = list->list[index];
    auto  deviceMgr = info->getDeviceManager();
    auto  device    = deviceMgr->createDevice(info);

    auto impl    = new ob_device();
    impl->device = device;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_device *ob_device_list_get_device_by_serial_number(const ob_device_list *list, const char *serial_number, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(serial_number);
    for(auto &info: list->list) {
        if(info->getDeviceSn() == serial_number) {
            auto deviceMgr = info->getDeviceManager();
            auto device    = deviceMgr->createDevice(info);
            auto impl      = new ob_device();
            impl->device   = device;
            return impl;
        }
    }
    throw libobsensor::invalid_value_exception("Device not found by serial number: " + std::string(serial_number));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, serial_number)

ob_device *ob_device_list_get_device_by_uid(const ob_device_list *list, const char *uid, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(uid);
    for(auto &info: list->list) {
        auto deviceUid = info->getUid();
        if(deviceUid == uid
#if defined(__linux__)
           // For linux platform, the usb device uid is like "bus-port-dev", try to rm the dev part to match the uid
           || deviceUid.substr(0, deviceUid.find_last_of('-')) == uid
#endif
        ) {
            auto deviceMgr = info->getDeviceManager();
            auto device    = deviceMgr->createDevice(info);
            auto impl      = new ob_device();
            impl->device   = device;
            return impl;
        }
    }
    throw libobsensor::invalid_value_exception("Device not found by UID: " + std::string(uid));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, uid)

void ob_delete_device(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    delete device;
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

ob_sensor_list *ob_device_get_sensor_list(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto impl         = new ob_sensor_list();
    impl->device      = device->device;
    impl->sensorTypes = device->device->getSensorTypeList();
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

ob_sensor *ob_device_get_sensor(ob_device *device, ob_sensor_type type, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto sensorTypelist = device->device->getSensorTypeList();
    auto iter           = std::find(sensorTypelist.begin(), sensorTypelist.end(), type);
    if(iter == sensorTypelist.end()) {
        throw libobsensor::invalid_value_exception("Sensor not found by type: " + std::to_string(type));
    }
    auto impl    = new ob_sensor();
    impl->device = device->device;
    impl->type   = type;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, type)

void ob_device_set_int_property(ob_device *device, ob_property_id property_id, int32_t value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

int32_t ob_device_get_int_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device, property_id)

ob_int_property_range ob_device_get_int_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_int_property_range(), device, property_id)

void ob_device_set_float_property(ob_device *device, ob_property_id property_id, float value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

float ob_device_get_float_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<float>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0.0f, device, property_id)

ob_float_property_range ob_device_get_float_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<float>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_float_property_range(), device, property_id)

void ob_device_set_bool_property(ob_device *device, ob_property_id property_id, bool value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

bool ob_device_get_bool_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id)

ob_bool_property_range ob_device_get_bool_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_bool_property_range(), device, property_id)

void ob_device_set_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto                 propServer = device->device->getPropertyServer();
    std::vector<uint8_t> dataVec(data, data + data_size);
    propServer->setStructureData(property_id, dataVec, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

void ob_device_get_structured_data(ob_device *device, ob_property_id property_id, uint8_t *data, uint32_t *data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto  propServer   = device->device->getPropertyServer();
    auto &firmwareData = propServer->getStructureData(property_id, libobsensor::PROP_ACCESS_USER);

    memcpy(data, firmwareData.data(), firmwareData.size());
    *data_size = static_cast<uint32_t>(firmwareData.size());
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

void ob_device_get_raw_data(ob_device *device, ob_property_id property_id, ob_get_data_callback cb, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->getRawData(
        property_id,
        [=](OBDataTranState state, OBDataChunk *dataChunk) {
            if(cb) {
                cb(state, dataChunk, user_data);
            }
        },
        libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, user_data)

uint32_t ob_device_get_supported_property_count(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return static_cast<int>(propServer->getAvailableProperties(libobsensor::PROP_ACCESS_USER).size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

ob_property_item ob_device_get_supported_property_item(const ob_device *device, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto properties = propServer->getAvailableProperties(libobsensor::PROP_ACCESS_USER);
    VALIDATE_UNSIGNED_INDEX(index, properties.size());
    return properties.at(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_property_item(), device, index)

bool ob_device_is_property_supported(const ob_device *device, ob_property_id property_id, ob_permission_type permission, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto operationType = libobsensor::PROP_OP_READ_WRITE;
    if(permission == OB_PERMISSION_READ) {
        operationType = libobsensor::PROP_OP_READ;
    }
    else if(permission == OB_PERMISSION_WRITE) {
        operationType = libobsensor::PROP_OP_WRITE;
    }

    auto propServer = device->device->getPropertyServer();
    return propServer->isPropertySupported(property_id, operationType, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id, permission)

bool ob_device_is_global_timestamp_supported(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    return device->device->isComponentExists(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_device_enable_global_timestamp(ob_device *device, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto globalTimestampFilter = device->device->getComponentT<libobsensor::IGlobalTimestampFitter>(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER);
    globalTimestampFilter->enable(enable);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, enable)

void ob_device_update_firmware(ob_device *device, const char *path, ob_device_fw_update_callback callback, bool async, void *user_data,
                               ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(path);
    VALIDATE_NOT_NULL(callback);
    auto data = libobsensor::utils::readFile(path);

    device->device->updateFirmware(
        data,
        [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) {  //
            callback(status, message, percent, user_data);
        },
        async);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, path, callback, async, user_data)

void ob_device_update_firmware_from_data(ob_device *device, const uint8_t *data, uint32_t data_size, ob_device_fw_update_callback callback, bool async,
                                         void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(callback);
    auto dataVec = std::vector<uint8_t>(data, data + data_size);
    device->device->updateFirmware(
        dataVec,
        [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) {  //
            callback(status, message, percent, user_data);
        },
        async);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, data, data_size, callback, async, user_data)

void ob_device_reboot(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    device->device->reboot();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

ob_device_state ob_device_get_device_state(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    return devMonitor->getCurrentDeviceState();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void ob_device_set_state_changed_callback(ob_device *device, ob_device_state_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    devMonitor->registerStateChangedCallback([callback, user_data](OBDeviceState state, const std::string &message) {  //
        callback(state, message.c_str(), user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_enable_heartbeat(ob_device *device, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);

    if(enable) {
        devMonitor->enableHeartbeat();
    }
    else {
        devMonitor->disableHeartbeat();
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(device, enable)

void ob_device_send_and_receive_data(ob_device *device, const uint8_t *send_data, uint32_t send_data_size, uint8_t *receive_data, uint32_t *receive_data_size,
                                     ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(send_data);
    VALIDATE_NOT_NULL(receive_data);
    VALIDATE_NOT_NULL(receive_data_size);

    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    devMonitor->sendAndReceiveData(send_data, send_data_size, receive_data, receive_data_size);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, send_data, send_data_size, receive_data, receive_data_size)

ob_device_info *ob_device_get_device_info(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto info  = device->device->getInfo();
    auto impl  = new ob_device_info();
    impl->info = info;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_delete_device_info(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    delete info;
}
HANDLE_EXCEPTIONS_NO_RETURN(info)

const char *ob_device_info_get_name(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->fullName_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

int ob_device_info_get_pid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->pid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

int ob_device_info_get_vid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->vid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

const char *ob_device_info_get_uid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->uid_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_serial_number(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->deviceSn_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_firmware_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->fwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_hardware_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->hwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_connection_type(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->connectionType_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_ip_address(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    auto netDeviceInfo = std::static_pointer_cast<const libobsensor::NetDeviceInfo>(info->info);
    if(!netDeviceInfo) {
        throw libobsensor::invalid_value_exception("Not a network device");
    }
    return netDeviceInfo->ipAddress_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_supported_min_sdk_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->supportedSdkVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_asicName(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->asicName_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

ob_device_type ob_device_info_get_device_type(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return static_cast<ob_device_type>(info->info->type_);
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_DEVICE_TYPE_UNKNOWN, info)

bool ob_device_is_extension_info_exist(const ob_device *device, const char *info_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(info_key);
    return device->device->isExtensionInfoExists(info_key);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, info_key)

const char *ob_device_get_extension_info(const ob_device *device, const char *info_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(info_key);
    const auto &extensionInfo = device->device->getExtensionInfo(info_key);
    if(extensionInfo.empty()) {
        return nullptr;
    }
    return extensionInfo.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN("", device, info_key)

ob_camera_param_list *ob_device_get_calibration_camera_param_list(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto algParamManager         = device->device->getComponentT<libobsensor::IAlgParamManager>(libobsensor::OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto calibrationCameraParams = algParamManager->getCalibrationCameraParamList();
    auto impl                    = new ob_camera_param_list();
    impl->paramList              = calibrationCameraParams;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

uint32_t ob_camera_param_list_get_count(ob_camera_param_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    return static_cast<uint32_t>(list->paramList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

ob_camera_param ob_camera_param_list_get_param(ob_camera_param_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->paramList.size());

    return list->paramList.at(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_camera_param(), list, index)

void ob_delete_camera_param_list(ob_camera_param_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    delete list;
}
HANDLE_EXCEPTIONS_NO_RETURN(list)

#ifdef __cplusplus
}
#endif
