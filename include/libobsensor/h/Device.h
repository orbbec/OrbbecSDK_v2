﻿/**
 * @file Device.h
 * @brief Device-related functions, including operations such as obtaining and creating a device, setting and obtaining device property, and obtaining sensors
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ObTypes.h"
#include "Property.h"
#include "MultipleDevices.h"
#include "Advanced.h"

/**
 * @brief Delete a device.
 *
 * @param[in] device The device to be deleted.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_delete_device(ob_device *device, ob_error **error);

/**
 * @brief List all sensors.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_sensor_list* The list of all sensors.
 */
OB_EXPORT ob_sensor_list *ob_device_get_sensor_list(const ob_device *device, ob_error **error);

/**
 * @brief Get a device's sensor.
 *
 * @param[in] device The device object.
 * @param[in] type The type of sensor to get.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_sensor* The acquired sensor.
 */
OB_EXPORT ob_sensor *ob_device_get_sensor(ob_device *device, ob_sensor_type type, ob_error **error);

/**
 * @brief Set an integer type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The ID of the property to be set.
 * @param[in] value The property value to be set.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_int_property(ob_device *device, ob_property_id property_id, int32_t value, ob_error **error);

/**
 * @brief Get an integer type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property ID.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int32_t The property value.
 */
OB_EXPORT int32_t ob_device_get_int_property(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Get the integer type of device property range.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property id.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return The property range.
 */
OB_EXPORT ob_int_property_range ob_device_get_int_property_range(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Set a float type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The ID of the property to be set.
 * @param[in] value The property value to be set.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_float_property(ob_device *device, ob_property_id property_id, float value, ob_error **error);

/**
 * @brief Get a float type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property ID.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return float The property value.
 */
OB_EXPORT float ob_device_get_float_property(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Get the float type of device property range.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property id.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return The property range.
 */
OB_EXPORT ob_float_property_range ob_device_get_float_property_range(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Set a boolean type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The ID of the property to be set.
 * @param[in] value The property value to be set.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_bool_property(ob_device *device, ob_property_id property_id, bool value, ob_error **error);

/**
 * @brief Get a boolean type of device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property ID.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return bool The property value.
 */
OB_EXPORT bool ob_device_get_bool_property(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Get the boolean type of device property range.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property id.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return The property range.
 */
OB_EXPORT ob_bool_property_range ob_device_get_bool_property_range(ob_device *device, ob_property_id property_id, ob_error **error);

/**
 * @brief Set structured data.
 *
 * @param[in] device The device object.
 * @param[in] property_id The ID of the property to be set.
 * @param[in] data The property data to be set.
 * @param[in] data_size The size of the property to be set.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t data_size, ob_error **error);

/**
 * @brief Get structured data of a device property.
 *
 * @param[in] device The device object.
 * @param[in] property_id The ID of the property.
 * @param[out] data The obtained property data.
 * @param[out] data_size The size of the obtained property data.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_get_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t *data_size, ob_error **error);

/**
 * @brief Get the number of properties supported by the device.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return The number of properties supported by the device.
 */
OB_EXPORT uint32_t ob_device_get_supported_property_count(const ob_device *device, ob_error **error);

/**
 * @brief Get the type of property supported by the device.
 *
 * @param[in] device The device object.
 * @param[in] index The property index.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return The type of property supported by the device.
 */
OB_EXPORT ob_property_item ob_device_get_supported_property_item(const ob_device *device, uint32_t index, ob_error **error);

/**
 * @brief Check if a device property permission is supported.
 *
 * @param[in] device The device object.
 * @param[in] property_id The property id.
 * @param[in] permission The type of permission that needs to be interpreted.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return Whether the property permission is supported.
 */
OB_EXPORT bool ob_device_is_property_supported(const ob_device *device, ob_property_id property_id, ob_permission_type permission, ob_error **error);

/**
 * @brief Check if the device supports global timestamp.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return bool Whether the device supports global timestamp.
 */
OB_EXPORT bool ob_device_is_global_timestamp_supported(const ob_device *device, ob_error **error);

/**
 * @brief Update the device firmware.
 *
 * @param[in] device The device object.
 * @param[in] path The firmware path.
 * @param[in] callback The firmware upgrade progress callback.
 * @param[in] async Whether to execute asynchronously.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_update_firmware(ob_device *device, const char *path, ob_device_fw_update_callback callback, bool async, void *user_data,
                                         ob_error **error);

/**
 * @brief Update the device firmware from data.
 *
 * @param[in] device The device object.
 * @param[in] data The firmware file data.
 * @param[in] data_size The firmware file size.
 * @param[in] callback The firmware upgrade progress callback.
 * @param[in] async Whether to execute asynchronously.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_update_firmware_from_data(ob_device *device, const uint8_t *data, uint32_t data_size, ob_device_fw_update_callback callback,
                                                   bool async, void *user_data, ob_error **error);

/**
 * @brief Get the current device status.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 *
 * @return ob_device_state The device state information.
 */
OB_EXPORT ob_device_state ob_device_get_device_state(const ob_device *device, ob_error **error);

/**
 * @brief Set the device state changed callback.
 *
 * @param[in] device The device object.
 * @param[in] callback The callback function to be called when the device status changes.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_state_changed_callback(ob_device *device, ob_device_state_callback callback, void *user_data, ob_error **error);

/**
 * @brief Device reboot
 * @attention The device will be disconnected and reconnected. After the device is disconnected, the interface access to the device handle may be abnormal.
 * Please use the ob_delete_device interface to delete the handle directly. After the device is reconnected, it can be obtained again.
 *
 * @param[in] device Device object
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_reboot(ob_device *device, ob_error **error);

/**
 * @brief Get device information.
 *
 * @param[in] device The device to obtain information from.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_device_info* The device information.
 */
OB_EXPORT ob_device_info *ob_device_get_device_info(const ob_device *device, ob_error **error);

/**
 * @brief Delete device information.
 *
 * @param[in] info The device information to be deleted.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_delete_device_info(ob_device_info *info, ob_error **error);

/**
 * @brief Get device name
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* return the device name
 */
OB_EXPORT const char *ob_device_info_get_name(const ob_device_info *info, ob_error **error);

/**
* @brief Get device pid

 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int return the device pid
*/
OB_EXPORT int ob_device_info_get_pid(const ob_device_info *info, ob_error **error);

/**
 * @brief Get device vid
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int return device vid
 */
OB_EXPORT int ob_device_info_get_vid(const ob_device_info *info, ob_error **error);

/**
 * @brief Get device uid
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* return device uid
 */
OB_EXPORT const char *ob_device_info_get_uid(const ob_device_info *info, ob_error **error);

/**
 * @brief Get device serial number
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* return device serial number
 */
OB_EXPORT const char *ob_device_info_get_serial_number(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the firmware version number
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int return the firmware version number
 */
OB_EXPORT const char *ob_device_info_get_firmware_version(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the device connection type
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The connection type，currently supports："USB", "USB1.0", "USB1.1", "USB2.0", "USB2.1", "USB3.0", "USB3.1", "USB3.2", "Ethernet"
 */
OB_EXPORT const char *ob_device_info_get_connection_type(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the device IP address
 *
 * @attention Only valid for network devices, otherwise it will return "0.0.0.0"
 *
 * @param info Device Information
 * @param error Pointer to an error object that will be set if an error occurs.
 * @return const char* The IP address，such as "192.168.1.10"
 */
OB_EXPORT const char *ob_device_info_get_ip_address(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the hardware version number
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The hardware version number
 */
OB_EXPORT const char *ob_device_info_get_hardware_version(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the device extension information.
 * @brief Extension information is a set of key-value pair of string, user cat get the information by the key.
 *
 * @param[in] info Device Information
 * @param[in] info_key The key of the device extension information.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The device extension information
 */
OB_EXPORT const char *ob_device_info_get_extension_info(const ob_device_info *info, const char *info_key, ob_error **error);

/**
 * @brief Get the minimum SDK version number supported by the device
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The minimum SDK version number supported by the device
 */
OB_EXPORT const char *ob_device_info_get_supported_min_sdk_version(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the chip name
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The ASIC name
 */
OB_EXPORT const char *ob_device_info_get_asicName(const ob_device_info *info, ob_error **error);

/**
 * @brief Get the device type
 *
 * @param[in] info Device Information
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_device_type The device type
 */
OB_EXPORT ob_device_type ob_device_info_get_device_type(const ob_device_info *info, ob_error **error);

/**
 * @brief Delete a device list.
 *
 * @param[in] list The device list object to be deleted.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_delete_device_list(ob_device_list *list, ob_error **error);

/**
 * @brief Get the number of devices
 *
 * @param[in] list Device list object
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return uint32_t return the number of devices
 */
OB_EXPORT uint32_t ob_device_list_get_device_count(const ob_device_list *list, ob_error **error);

/**
 * @brief Get device name
 *
 * @param[in] list Device list object
 * @param[in] index Device index
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* return device name
 */
OB_EXPORT const char *ob_device_list_get_device_name(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get the pid of the specified device
 *
 * @param[in] list Device list object
 * @param[in] index Device index
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int return the device pid
 */
OB_EXPORT int ob_device_list_get_device_pid(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get the vid of the specified device
 *
 * @param[in] list Device list object
 * @param[in] index Device index
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return int return device vid
 */
OB_EXPORT int ob_device_list_get_device_vid(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get the uid of the specified device
 *
 * @param[in] list Device list object
 * @param[in] index Device index
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* return the device uid
 */
OB_EXPORT const char *ob_device_list_get_device_uid(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get the serial number of the specified device.
 *
 * @param[in] list Device list object.
 * @param[in] index Device index.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* The device UID.
 */
OB_EXPORT const char *ob_device_list_get_device_serial_number(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get device connection type
 *
 * @param[in] list Device list object
 * @param[in] index Device index
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return const char* returns the device connection type，currently supports："USB", "USB1.0", "USB1.1", "USB2.0", "USB2.1", "USB3.0", "USB3.1", "USB3.2",
 * "Ethernet"
 */
OB_EXPORT const char *ob_device_list_get_device_connection_type(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Get device ip address
 *
 * @attention Only valid for network devices, otherwise it will return "0.0.0.0".
 *
 * @param list Device list object
 * @param index Device index
 * @param error Pointer to an error object that will be set if an error occurs.
 * @return const char* returns the device ip address，such as "192.168.1.10"
 */
OB_EXPORT const char *ob_device_list_get_device_ip_address(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Create a device.
 *
 * @attention If the device has already been acquired and created elsewhere, repeated acquisitions will return an error.
 *
 * @param[in] list Device list object.
 * @param[in] index The index of the device to create.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_device* The created device.
 *
 */
OB_EXPORT ob_device *ob_device_list_get_device(const ob_device_list *list, uint32_t index, ob_error **error);

/**
 * @brief Create a device.
 *
 * @attention If the device has already been acquired and created elsewhere, repeated acquisitions will return an error.
 *
 * @param[in] list Device list object.
 * @param[in] serial_number The serial number of the device to create.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_device* The created device.
 */
OB_EXPORT ob_device *ob_device_list_get_device_by_serial_number(const ob_device_list *list, const char *serial_number, ob_error **error);

/**
 * @brief Create device by uid
 * @brief On Linux platform, the uid of the device is composed of bus-port-dev, for example 1-1.2-1. But the SDK will remove the dev number and only keep the
 * bus-port as the uid to create the device, for example 1-1.2, so that we can create a device connected to the specified USB port. Similarly, users can also
 * directly pass in bus-port as uid to create device.
 *
 * @attention If the device has already been acquired and created elsewhere, repeated acquisitions will return an error.
 *
 * @param[in] list Device list object.
 * @param[in] uid The UID of the device to create.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 * @return ob_device* The created device.
 */
OB_EXPORT ob_device *ob_device_list_get_device_by_uid(const ob_device_list *list, const char *uid, ob_error **error);

#ifdef __cplusplus
}
#endif