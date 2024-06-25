#include "openobsdk/h/Device.h"
#include "ImplTypes.hpp"
#include "exception/ObException.hpp"

#include "IDeviceEnumerator.hpp"
#include "IProperty.hpp"
#include "IDevice.hpp"
#include "device/component/property/PropertyAccessor.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void ob_delete_device_list(ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    delete list;
}
HANDLE_EXCEPTIONS_NO_RETURN(list)

uint32_t ob_device_list_get_device_count(ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    return static_cast<uint32_t>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

const char *ob_device_list_get_device_name(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getName().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

int ob_device_list_get_device_pid(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getPid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

int ob_device_list_get_device_vid(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getVid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

const char *ob_device_list_get_device_uid(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size() - 1);
    auto &info = list->list[index];
    return info->getUid().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_serial_number(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getDeviceSn().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_connection_type(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getConnectionType().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_ip_address(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    // auto &info = list->list[index];
    throw libobsensor::not_implemented_exception("ob_device_list_get_device_ip_address not implemented yet!");
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_device *ob_device_list_get_device(ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info   = list->list[index];
    auto  device = info->createDevice();

    auto impl    = new ob_device();
    impl->device = device;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_device *ob_device_list_get_device_by_serial_number(ob_device_list *list, const char *serial_number, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(serial_number);
    for(auto &info: list->list) {
        if(info->getDeviceSn() == serial_number) {
            auto device  = info->createDevice();
            auto impl    = new ob_device();
            impl->device = device;
            return impl;
        }
    }
    throw libobsensor::invalid_value_exception("Device not found by serial number: " + std::string(serial_number));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, serial_number)

ob_device *ob_device_list_get_device_by_uid(ob_device_list *list, const char *uid, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(uid);
    for(auto &info: list->list) {
        if(info->getUid() == uid) {
            auto device  = info->createDevice();
            auto impl    = new ob_device();
            impl->device = device;
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

ob_sensor_list *ob_device_get_sensor_list(ob_device *device, ob_error **error) BEGIN_API_CALL {
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
    auto accessor = device->device->getPropertyAccessor();
    accessor->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

int32_t ob_device_get_int_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    return accessor->getPropertyValueT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device, property_id)

ob_int_property_range ob_device_get_int_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    auto range    = accessor->getPropertyRangeT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_int_property_range(), device, property_id)

void ob_device_set_float_property(ob_device *device, ob_property_id property_id, float value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    accessor->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

float ob_device_get_float_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    return accessor->getPropertyValueT<float>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0.0f, device, property_id)

ob_float_property_range ob_device_get_float_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    auto range    = accessor->getPropertyRangeT<float>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_float_property_range(), device, property_id)

void ob_device_set_bool_property(ob_device *device, ob_property_id property_id, bool value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    accessor->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

bool ob_device_get_bool_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    return accessor->getPropertyValueT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id)

ob_bool_property_range ob_device_get_bool_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    auto range    = accessor->getPropertyRangeT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_bool_property_range(), device, property_id)

void ob_device_set_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto                 accessor = device->device->getPropertyAccessor();
    std::vector<uint8_t> dataVec(data, data + data_size);
    accessor->setStructureData(property_id, dataVec, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

void ob_device_get_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t *data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto  accessor     = device->device->getPropertyAccessor();
    auto &firmwareData = accessor->getStructureData(property_id, libobsensor::PROP_ACCESS_USER);

    data       = firmwareData.data();
    *data_size = static_cast<uint32_t>(firmwareData.size());
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

uint32_t ob_device_get_supported_property_count(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    return static_cast<int>(accessor->getAvailableProperties(libobsensor::PROP_ACCESS_USER).size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

ob_property_item ob_device_get_supported_property_item(ob_device *device, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    auto propertyVec=accessor->getAvailableProperties(libobsensor::PROP_ACCESS_USER);
    return accessor->getAvailableProperties(libobsensor::PROP_ACCESS_USER).at(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_property_item(), device, index)

bool ob_device_is_property_supported(ob_device *device, ob_property_id property_id, ob_permission_type permission, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto accessor = device->device->getPropertyAccessor();
    return accessor->checkProperty(property_id, permission, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id, permission);

// todo: implement below:
// bool ob_device_is_global_timestamp_supported(ob_device *device, ob_error **error);

// void ob_device_update_firmware(ob_device *device, const char *path, ob_device_fw_update_callback callback, bool async, void *user_data, ob_error **error);
// void ob_device_upgrade_firmware_from_data(ob_device *device, const char *data, uint32_t data_size, ob_device_fw_update_callback callback, bool async,
//                                           void *user_data, ob_error **error);
// ob_device_state ob_device_get_device_state(ob_device *device, ob_error **error);
// void            ob_device_set_state_changed_callback(ob_device *device, ob_device_state_callback callback, void *user_data, ob_error **error);
// void            ob_device_reboot(ob_device *device, ob_error **error);

ob_device_info *ob_device_get_device_info(ob_device *device, ob_error **error) BEGIN_API_CALL {
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

const char *ob_device_info_get_name(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->name_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

int ob_device_info_get_pid(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->pid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

int ob_device_info_get_vid(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->vid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

const char *ob_device_info_get_uid(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->uid_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_serial_number(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->deviceSn_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_firmware_version(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->fwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_hardware_version(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->hwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_connection_type(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->connectionType_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_ip_address(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    // return info->info->ipAddress_.c_str();
    // todo: implement this
    throw libobsensor::not_implemented_exception("ob_device_info_get_ip_address not implemented");
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_supported_min_sdk_version(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->supportedSdkVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_asicName(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->asicName_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

ob_device_type ob_device_info_get_device_type(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return static_cast<ob_device_type>(info->info->type_);
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_DEVICE_TYPE_UNKNOWN, info)

const char *ob_device_info_get_extension_info(ob_device_info *info, const char *info_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    VALIDATE_NOT_NULL(info_key);
    // auto iter = info->info->extensionInfo_.find(info_key);
    // if(iter != info->info->extensionInfo_.end()) {
    //     return iter->second.c_str();
    // }
    // return nullptr;
    throw libobsensor::not_implemented_exception("ob_device_info_get_extension_info not implemented");
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info, info_key)

#ifdef __cplusplus
}
#endif