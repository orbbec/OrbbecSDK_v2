﻿/**
 * @file Device.hpp
 * @brief Device related types, including operations such as getting and creating a device, setting and obtaining device attributes, and obtaining sensors
 *
 */
#pragma once
#include "Types.hpp"

#include "openobsdk/h/Property.h"
#include "openobsdk/h/Device.h"
#include "openobsdk/hpp/Filter.hpp"
#include "openobsdk/hpp/Sensor.hpp"
#include "Error.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ob {

class DeviceInfo;
class SensorList;
class DevicePresetList;

class Device {
protected:
    ob_device_t *impl_ = nullptr;

public:
    /**
     * @brief Describe the entity of the RGBD camera, representing a specific model of RGBD camera
     */
    explicit Device(ob_device_t *impl) : impl_(impl) {}

    Device(Device &&other) noexcept : impl_(other.impl_) {
        other.impl_ = nullptr;
    }

    Device &operator=(Device &&other) noexcept {
        if(this != &other) {
            ob_error *error = nullptr;
            ob_delete_device(impl_, &error);
            Error::handle(&error, false);
            impl_       = other.impl_;
            other.impl_ = nullptr;
        }
        return *this;
    }

    Device(const Device &)            = delete;
    Device &operator=(const Device &) = delete;

    virtual ~Device() noexcept {
        ob_error *error = nullptr;
        ob_delete_device(impl_, &error);
        Error::handle(&error, false);
    }

    /**
     * @brief Get device information
     *
     * @return std::shared_ptr<DeviceInfo> return device information
     */
    std::shared_ptr<DeviceInfo> getDeviceInfo() {
        ob_error *error = nullptr;
        auto      info  = ob_device_get_device_info(impl_, &error);
        Error::handle(&error);
        return std::make_shared<DeviceInfo>(info);
    }

    /**
     * @brief Get device sensor list
     *
     * @return std::shared_ptr<SensorList> return the sensor list
     */
    std::shared_ptr<SensorList> getSensorList() {
        ob_error *error = nullptr;
        auto      list  = ob_device_get_sensor_list(impl_, &error);
        Error::handle(&error);
        return std::make_shared<SensorList>(list);
    }

    /**
     * @brief Get specific type of sensor
     * if device not open, SDK will automatically open the connected device and return to the instance
     *
     * @return std::shared_ptr<Sensor> return the sensor example, if the device does not have the device,return nullptr
     */
    std::shared_ptr<Sensor> getSensor(OBSensorType type) {
        ob_error *error  = nullptr;
        auto      sensor = ob_device_get_sensor(impl_, type, &error);
        Error::handle(&error);
        return std::make_shared<Sensor>(sensor);
    }

    /**
     * @brief Set int type of device property
     *
     * @param propertyId Property id
     * @param property Property to be set
     */
    void setIntProperty(OBPropertyID propertyId, int32_t property) {
        ob_error *error = nullptr;
        ob_device_set_int_property(impl_, propertyId, property, &error);
        Error::handle(&error);
    }

    /**
     * @brief Set float type of device property
     *
     * @param propertyId Property id
     * @param property Property to be set
     */
    void setFloatProperty(OBPropertyID propertyId, float property) {
        ob_error *error = nullptr;
        ob_device_set_float_property(impl_, propertyId, property, &error);
        Error::handle(&error);
    }

    /**
     * @brief Set bool type of device property
     *
     * @param propertyId Property id
     * @param property Property to be set
     */
    void setBoolProperty(OBPropertyID propertyId, bool property) {
        ob_error *error = nullptr;
        ob_device_set_bool_property(impl_, propertyId, property, &error);
        Error::handle(&error);
    }

    /**
     * @brief Get int type of device property
     *
     * @param propertyId Property id
     * @return int32_t Property to get
     */
    int32_t getIntProperty(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      value = ob_device_get_int_property(impl_, propertyId, &error);
        Error::handle(&error);
        return value;
    }

    /**
     * @brief Get float type of device property
     *
     * @param propertyId Property id
     * @return float Property to get
     */
    float getFloatProperty(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      value = ob_device_get_float_property(impl_, propertyId, &error);
        Error::handle(&error);
        return value;
    }

    /**
     * @brief Get bool type of device property
     *
     * @param propertyId Property id
     * @return bool Property to get
     */
    bool getBoolProperty(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      value = ob_device_get_bool_property(impl_, propertyId, &error);
        Error::handle(&error);
        return value;
    }

    /**
     * @brief Get int type device property range (including current value and default value)
     *
     * @param propertyId Property id
     * @return OBIntPropertyRange Property range
     */
    OBIntPropertyRange getIntPropertyRange(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      range = ob_device_get_int_property_range(impl_, propertyId, &error);
        Error::handle(&error);
        return range;
    }

    /**
     * @brief Get float type device property range((including current value and default value)
     *
     * @param propertyId Property id
     * @return OBFloatPropertyRange Property range
     */
    OBFloatPropertyRange getFloatPropertyRange(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      range = ob_device_get_float_property_range(impl_, propertyId, &error);
        Error::handle(&error);
        return range;
    }

    /**
     * @brief Get bool type device property range (including current value and default value)
     *
     * @param propertyId The ID of the property
     * @return OBBoolPropertyRange The range of the property
     */
    OBBoolPropertyRange getBoolPropertyRange(OBPropertyID propertyId) {
        ob_error *error = nullptr;
        auto      range = ob_device_get_bool_property_range(impl_, propertyId, &error);
        Error::handle(&error);
        return range;
    }

    /**
     * @brief Set the structured data type of a device property
     *
     * @param propertyId The ID of the property
     * @param data The data to set
     * @param dataSize The size of the data to set
     */
    void setStructuredData(OBPropertyID propertyId, const uint8_t *data, uint32_t dataSize) {
        ob_error *error = nullptr;
        ob_device_set_structured_data(impl_, propertyId, data, dataSize, &error);
        Error::handle(&error);
    }

    /**
     * @brief Get the structured data type of a device property
     *
     * @param propertyId The ID of the property
     * @param data The property data obtained
     * @param dataSize The size of the data obtained
     */
    void getStructuredData(OBPropertyID propertyId, uint8_t *data, uint32_t *dataSize) {
        ob_error *error = nullptr;
        ob_device_get_structured_data(impl_, propertyId, data, dataSize, &error);
        Error::handle(&error);
    }

    /**
     * @brief Get the number of properties supported by the device
     *
     * @return The number of supported properties
     */
    uint32_t getSupportedPropertyCount() {
        ob_error *error = nullptr;
        auto      count = ob_device_get_supported_property_count(impl_, &error);
        Error::handle(&error);
        return count;
    }

    /**
     * @brief Get the supported properties of the device
     *
     * @param index The index of the property
     * @return The type of supported property
     */
    OBPropertyItem getSupportedProperty(uint32_t index) {
        ob_error *error = nullptr;
        auto      item  = ob_device_get_supported_property_item(impl_, index, &error);
        Error::handle(&error);
        return item;
    }

    /**
     * @brief Check if a property permission is supported
     *
     * @param propertyId The ID of the property
     * @param permission The read and write permissions to check
     * @return Whether the property permission is supported
     */
    bool isPropertySupported(OBPropertyID propertyId, OBPermissionType permission) {
        ob_error *error  = nullptr;
        auto      result = ob_device_is_property_supported(impl_, propertyId, permission, &error);
        Error::handle(&error);
        return result;
    }

    /**
     * @brief Check if the global timestamp is supported for the device
     *
     * @return Whether the global timestamp is supported
     */
    bool isGlobalTimestampSupported() {
        ob_error *error  = nullptr;
        auto      result = ob_device_is_global_timestamp_supported(impl_, &error);
        Error::handle(&error);
        return result;
    }

    /**
     * @brief Upgrade the device firmware
     *
     * @param filePath Firmware path
     * @param callback  Firmware upgrade progress and status callback
     * @param async    Whether to execute asynchronously
     */
    void deviceUpgrade(const char *filePath, DeviceUpgradeCallback callback, bool async = true){
        ob_error *error = nullptr;
        ob_device_update_firmware(impl_, filePath, callback, async, &error);
        Error::handle(&error);
    }

    /**
     * \if English
     * @brief Upgrade the device firmware
     *
     * @param fileData Firmware file data
     * @param fileSize Firmware file size
     * @param callback  Firmware upgrade progress and status callback
     * @param async    Whether to execute asynchronously
     */
    void deviceUpgradeFromData(const char *fileData, uint32_t fileSize, DeviceUpgradeCallback callback, bool async = true){
        ob_error *error = nullptr;
        ob_device_update_firmware_from_data(impl_, fileData, fileSize, callback, async, &error);
        Error::handle(&error);
    }

    /**
     * @brief Send files to the specified path on the device side [Asynchronouscallback]
     *
     * @param filePath Original file path
     * @param dstPath  Accept the save path on the device side
     * @param callback File transfer callback
     * @param async    Whether to execute asynchronously
     */
    void sendFile(const char *filePath, const char *dstPath, SendFileCallback callback, bool async = true);

    /**
     * @brief Get the current state
     * @return OBDeviceState device state information
     */
    OBDeviceState getDeviceState();

    /**
     * @brief Set the device state changed callbacks
     *
     * @param callback The callback function that is triggered when the device status changes (for example, the frame rate is automatically reduced or the
     * stream is closed due to high temperature, etc.)
     */
    void setDeviceStateChangedCallback(DeviceStateChangedCallback callback);

    /**
     * @brief Verify device authorization code
     *
     * @param authCode Authorization code
     * @return bool whether the activation is successful
     */
    bool activateAuthorization(const char *authCode);

    /**
     * @brief Write authorization code
     * @param[in] authCodeStr  Authorization code
     */
    void writeAuthorizationCode(const char *authCodeStr);

    /**
     * @brief Get the original parameter list of camera calibration saved in the device.
     *
     * @attention The parameters in the list do not correspond to the current open-current configuration. You need to select the parameters according to the
     * actual situation, and may need to do scaling, mirroring and other processing. Non-professional users are recommended to use the
     * Pipeline::getCameraParam() interface.
     *
     * @return std::shared_ptr<CameraParamList> camera parameter list
     */
    std::shared_ptr<CameraParamList> getCalibrationCameraParamList();

    /**
     * @brief Get current depth work mode
     *
     * @return ob_depth_work_mode Current depth work mode
     */
    OBDepthWorkMode getCurrentDepthWorkMode();

    /**
     * @brief Switch depth work mode by OBDepthWorkMode. Prefer invoke switchDepthWorkMode(const char *modeName) to switch depth mode
     *        when known the complete name of depth work mode.
     * @param[in] workMode Depth work mode come from ob_depth_work_mode_list which return by ob_device_get_depth_work_mode_list
     */
    OBStatus switchDepthWorkMode(const OBDepthWorkMode &workMode);

    /**
     * @brief Switch depth work mode by work mode name.
     *
     * @param[in] modeName Depth work mode name which equals to OBDepthWorkMode.name
     */
    OBStatus switchDepthWorkMode(const char *modeName);

    /**
     * @brief Request support depth work mode list
     * @return OBDepthWorkModeList list of ob_depth_work_mode
     */
    std::shared_ptr<OBDepthWorkModeList> getDepthWorkModeList();

    /**
     * @brief Device restart
     * @attention The device will be disconnected and reconnected. After the device is disconnected, the access to the Device object interface may be abnormal.
     *   Please delete the object directly and obtain it again after the device is reconnected.
     */
    void reboot();

    /**
     * @brief Device restart delay mode
     * @attention The device will be disconnected and reconnected. After the device is disconnected, the access to the Device object interface may be abnormal.
     *   Please delete the object directly and obtain it again after the device is reconnected.
     * Support devices: Gemini2 L
     *
     * @param[in] delayMs Time unit：ms。delayMs == 0：No delay；delayMs > 0, Delay millisecond connect to host device after reboot
     */
    void reboot(uint32_t delayMs);

    /**
     * @brief Get the supported multi device sync mode bitmap of the device.
     * @brief For example, if the return value is 0b00001100, it means the device supports @ref OB_MULTI_DEVICE_SYNC_MODE_PRIMARY and @ref
     * OB_MULTI_DEVICE_SYNC_MODE_SECONDARY. User can check the supported mode by the code:
     * ```c
     *   if(supported_mode_bitmap & OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN){
     *      //support OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN
     *   }
     *   if(supported_mode_bitmap & OB_MULTI_DEVICE_SYNC_MODE_STANDALONE){
     *     //support OB_MULTI_DEVICE_SYNC_MODE_STANDALONE
     *   }
     *   // and so on
     * ```
     * @return uint16_t return the supported multi device sync mode bitmap of the device.
     */
    uint16_t getSupportedMultiDeviceSyncModeBitmap();

    /**
     * @brief set the multi device sync configuration of the device.
     *
     * @param[in] config The multi device sync configuration.
     */
    void setMultiDeviceSyncConfig(const OBMultiDeviceSyncConfig &config);

    /**
     * @brief get the multi device sync configuration of the device.
     *
     * @return OBMultiDeviceSyncConfig return the multi device sync configuration of the device.
     */
    OBMultiDeviceSyncConfig getMultiDeviceSyncConfig();

    /**
     * @brief send the capture command to the device.
     * @brief The device will start one time image capture after receiving the capture command when it is in the @ref
     * OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING
     *
     * @attention The frequency of the user call this function multiplied by the number of frames per trigger should be less than the frame rate of the stream.
     * The number of frames per trigger can be set by @ref framesPerTrigger.
     * @attention For some models，receive and execute the capture command will have a certain delay and performance consumption, so the frequency of calling
     * this function should not be too high, please refer to the product manual for the specific supported frequency.
     * @attention If the device is not in the @ref OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING mode, device will ignore the capture command.
     */
    void triggerCapture();

    /**
     * @brief set the timestamp reset configuration of the device.
     */
    void setTimestampResetConfig(const OBDeviceTimestampResetConfig &config);

    /**
     * @brief get the timestamp reset configuration of the device.
     *
     * @return OBDeviceTimestampResetConfig return the timestamp reset configuration of the device.
     */
    OBDeviceTimestampResetConfig getTimestampResetConfig();

    /**
     * @brief send the timestamp reset command to the device.
     * @brief The device will reset the timer for calculating the timestamp for output frames to 0 after receiving the timestamp reset command when the
     * timestamp reset function is enabled. The timestamp reset function can be enabled by call @ref ob_device_set_timestamp_reset_config.
     * @brief Before calling this function, user should call @ref ob_device_set_timestamp_reset_config to disable the timestamp reset function (It is not
     * required for some models, but it is still recommended to do so for code compatibility).
     *
     * @attention If the stream of the device is started, the timestamp of the continuous frames output by the stream will jump once after the timestamp reset.
     * @attention Due to the timer of device is not high-accuracy, the timestamp of the continuous frames output by the stream will drift after a long time.
     * User can call this function periodically to reset the timer to avoid the timestamp drift, the recommended interval time is 60 minutes.
     */
    void timestampReset();

    /**
     *  @brief Alias for @ref timestampReset since it is more accurate.
     */
#define timerReset timestampReset

    /**
     * @brief synchronize the timer of the device with the host.
     * @brief After calling this function, the timer of the device will be synchronized with the host. User can call this function to multiple devices to
     * synchronize all timers of the devices.
     *
     * @attention If the stream of the device is started, the timestamp of the continuous frames output by the stream will may jump once after the timer
     * sync.
     * @attention Due to the timer of device is not high-accuracy, the timestamp of the continuous frames output by the stream will drift after a long time.
     * User can call this function periodically to synchronize the timer to avoid the timestamp drift, the recommended interval time is 60 minutes.
     *
     */
    void timerSyncWithHost();

    /**
     * @brief Load depth filter config from file.
     * @param filePath Path of the config file.
     */
    void loadDepthFilterConfig(const char *filePath);

    /**
     * @brief Reset depth filter config to device default define.
     */
    void resetDefaultDepthFilterConfig();

    /**
     * @brief Get current preset name
     * @brief The preset mean a set of parameters or configurations that can be applied to the device to achieve a specific effect or function.
     * @return const char* return the current preset name, it should be one of the preset names returned by @ref getAvailablePresetList.
     */
    const char *getCurrentPresetName();

    /**
     * @brief load the preset according to the preset name.
     * @attention After loading the preset, the settings in the preset will set to the device immediately. Therefore, it is recommended to re-read the device
     * settings to update the user program temporarily.
     * @param presetName The preset name to set. The name should be one of the preset names returned by @ref getAvailablePresetList.
     */
    void loadPreset(const char *presetName);

    /**
     * @brief Get available preset list
     * @brief The available preset list usually defined by the device manufacturer and restores on the device.
     * @brief User can load the custom preset by calling @ref loadPresetFromJsonFile to append the available preset list.
     *
     * @return DevicePresetList return the available preset list.
     */
    std::shared_ptr<DevicePresetList> getAvailablePresetList();

    /**
     * @brief Load custom preset from file.
     * @brief After loading the custom preset, the settings in the custom preset will set to the device immediately.
     * @brief After loading the custom preset, the available preset list will be appended with the custom preset and named as the file name.
     *
     * @attention The user should ensure that the custom preset file is adapted to the device and the settings in the file are valid.
     * @attention It is recommended to re-read the device settings to update the user program temporarily after successfully loading the custom preset.
     *
     * @param filePath The path of the custom preset file.
     */
    void loadPresetFromJsonFile(const char *filePath);

    /**
     * @brief Load custom preset from data.
     * @brief After loading the custom preset, the settings in the custom preset will set to the device immediately.
     * @brief After loading the custom preset, the available preset list will be appended with the custom preset and named as the @ref presetName.
     *
     * @attention The user should ensure that the custom preset data is adapted to the device and the settings in the data are valid.
     * @attention It is recommended to re-read the device settings to update the user program temporarily after successfully loading the custom preset.
     *
     * @param data The custom preset data.
     * @param size The size of the custom preset data.
     */
    void loadPresetFromJsonData(const char *presetName, const uint8_t *data, uint32_t size);

    /**
     * @brief Export current device settings as a preset json file.
     * @brief The exported preset file can be loaded by calling @ref loadPresetFromJsonFile to restore the device setting.
     * @brief After exporting the preset, a new preset named as the @ref filePath will be added to the available preset list.
     *
     * @param filePath The path of the preset file to be exported.
     */
    void exportSettingsAsPresetJsonFile(const char *filePath);

    /**
     * @brief Export current device settings as a preset json data.
     * @brief After exporting the preset, a new preset named as the @ref presetName will be added to the available preset list.
     *
     * @attention The memory of the data is allocated by the SDK, and will automatically be released by the SDK.
     * @attention The memory of the data will be reused by the SDK on the next call, so the user should copy the data to a new buffer if it needs to be
     * preserved.
     *
     * @param[out] data return the preset json data.
     * @param[out] dataSize return the size of the preset json data.
     */
    void exportSettingsAsPresetJsonData(const char *presetName, const uint8_t **data, uint32_t *dataSize);
};

/**
 * @brief A class describing device information, representing the name, id, serial number and other basic information of an RGBD camera.
 */
class DeviceInfo {
private:
    ob_device_info_t *impl_ = nullptr;

public:
    explicit DeviceInfo(ob_device_info_t *impl) : impl_(impl) {}
    ~DeviceInfo() noexcept {
        ob_error *error = nullptr;
        ob_delete_device_info(impl_, &error);
        Error::handle(&error, false);
    }

    /**
     * @brief Get device name
     *
     * @return const char * return the device name
     */
    const char *name() const {
        ob_error   *error = nullptr;
        const char *name  = ob_device_info_get_name(impl_, &error);
        Error::handle(&error);
        return name;
    }

    /**
     * @brief Get the pid of the device
     *
     * @return int return the pid of the device
     */
    int pid() {
        ob_error *error = nullptr;
        int       pid   = ob_device_info_get_pid(impl_, &error);
        Error::handle(&error);
        return pid;
    }

    /**
     * @brief Get the vid of the device
     *
     * @return int return the vid of the device
     */
    int vid() {
        ob_error *error = nullptr;
        int       vid   = ob_device_info_get_vid(impl_, &error);
        Error::handle(&error);
        return vid;
    }

    /**
     * @brief Get system assigned uid for distinguishing between different devices
     *
     * @return const char * return the uid of the device
     */
    const char *uid() const {
        ob_error   *error = nullptr;
        const char *uid   = ob_device_info_get_uid(impl_, &error);
        Error::handle(&error);
        return uid;
    }

    /**
     * @brief Get the serial number of the device
     *
     * @return const char * return the serial number of the device
     */
    const char *serialNumber() const {
        ob_error   *error = nullptr;
        const char *sn    = ob_device_info_get_serial_number(impl_, &error);
        Error::handle(&error);
        return sn;
    }

    /**
     * @brief Get the version number of the firmware
     *
     * @return const char* return the version number of the firmware
     */
    const char *firmwareVersion() const {
        ob_error   *error   = nullptr;
        const char *version = ob_device_info_get_firmware_version(impl_, &error);
        Error::handle(&error);
        return version;
    }

    /**
     * @brief Get the connection type of the device
     *
     * @return const char* the connection type of the device，currently supports："USB", "USB1.0", "USB1.1", "USB2.0", "USB2.1", "USB3.0", "USB3.1", "USB3.2",
     * "Ethernet"
     */
    const char *connectionType() const {
        ob_error   *error = nullptr;
        const char *type  = ob_device_info_get_connection_type(impl_, &error);
        Error::handle(&error);
        return type;
    }

    /**
     * @brief Get the IP address of the device
     *
     * @attention Only valid for network devices, otherwise it will return "0.0.0.0".
     *
     * @return const char* the IP address of the device, such as "192.168.1.10"
     */
    const char *ipAddress() const {
        ob_error   *error = nullptr;
        const char *ip    = ob_device_info_get_ip_address(impl_, &error);
        Error::handle(&error);
        return ip;
    }

    /**
     * @brief Get the version number of the hardware
     *
     * @return const char* the version number of the hardware
     */
    const char *hardwareVersion() const {
        ob_error   *error   = nullptr;
        const char *version = ob_device_info_get_hardware_version(impl_, &error);
        Error::handle(&error);
        return version;
    }

    /**
     * @brief Get the minimum version number of the SDK supported by the device
     *
     * @return const char* the minimum SDK version number supported by the device
     */
    const char *supportedMinSdkVersion() const {
        ob_error   *error   = nullptr;
        const char *version = ob_device_info_get_supported_min_sdk_version(impl_, &error);
        Error::handle(&error);
        return version;
    }

    /**
     * @brief Get information about extensions obtained from SDK supported by the device
     *
     * @return const char* Returns extended information about the device
     */
    const char *extensionInfo(const char *info_key) const {
        ob_error   *error = nullptr;
        const char *info  = ob_device_info_get_extension_info(impl_, info_key, &error);
        Error::handle(&error);
        return info;
    }

    /**
     * @brief Get chip type name
     *
     * @return const char* the chip type name
     */
    const char *asicName() const {
        ob_error   *error = nullptr;
        const char *name  = ob_device_info_get_asicName(impl_, &error);
        Error::handle(&error);
        return name;
    }

    /**
     * @brief Get the device type
     *
     * @return OBDeviceType the device type
     */
    OBDeviceType deviceType() {
        ob_error    *error = nullptr;
        OBDeviceType type  = ob_device_info_get_device_type(impl_, &error);
        Error::handle(&error);
        return type;
    }
};

/**
 * @brief Class representing a list of devices
 */
class DeviceList {
private:
    ob_device_list_t *impl_ = nullptr;

public:
    explicit DeviceList(ob_device_list_t *impl) : impl_(impl) {}
    ~DeviceList() noexcept {}

    /**
     * @brief Get the number of devices in the list
     *
     * @return uint32_t the number of devices in the list
     */
    uint32_t deviceCount();

    /**
     * @brief Get the PID of the device at the specified index
     *
     * @param index the index of the device
     * @return int the PID of the device
     */
    int pid(uint32_t index);

    /**
     * @brief Get the VID of the device at the specified index
     *
     * @param index the index of the device
     * @return int the VID of the device
     */
    int vid(uint32_t index);

    /**
     * @brief Get the UID of the device at the specified index
     *
     * @param index the index of the device
     * @return const char* the UID of the device
     */
    const char *uid(uint32_t index);

    /**
     * @brief Get the serial number of the device at the specified index
     *
     * @param index the index of the device
     * @return const char* the serial number of the device
     */
    const char *serialNumber(uint32_t index);

    /**
     * @brief Get device connection type
     *
     * @param index device index
     * @return const char* returns connection type，currently supports："USB", "USB1.0", "USB1.1", "USB2.0", "USB2.1", "USB3.0", "USB3.1", "USB3.2", "Ethernet"
     */
    const char *connectionType(uint32_t index);

    /**
     * @brief get the ip address of the device at the specified index
     *
     * @attention Only valid for network devices, otherwise it will return "0.0.0.0".
     *
     * @param index the index of the device
     * @return const char* the ip address of the device
     */
    const char *ipAddress(uint32_t index);

    /**
     * @brief Get the device object at the specified index
     *
     * @attention If the device has already been acquired and created elsewhere, repeated acquisition will throw an exception
     *
     * @param index the index of the device to create
     * @return std::shared_ptr<Device> the device object
     */
    std::shared_ptr<Device> getDevice(uint32_t index);

    /**
     * @brief Get the device object with the specified serial number
     *
     * @attention If the device has already been acquired and created elsewhere, repeated acquisition will throw an exception
     *
     * @param serialNumber the serial number of the device to create
     * @return std::shared_ptr<Device> the device object
     */
    std::shared_ptr<Device> getDeviceBySN(const char *serialNumber);

    /**
     * @brief Get the specified device object from the device list by uid
     * @brief On Linux platform, the uid of the device is composed of bus-port-dev, for example 1-1.2-1. But the SDK will remove the dev number and only keep
     * the bus-port as the uid to create the device, for example 1-1.2, so that we can create a device connected to the specified USB port. Similarly, users can
     * also directly pass in bus-port as uid to create device.
     *
     * @attention If the device has been acquired and created elsewhere, repeated acquisition will throw an exception
     *
     * @param uid The uid of the device to be created
     * @return std::shared_ptr<Device> returns the device object
     */
    std::shared_ptr<Device> getDeviceByUid(const char *uid);
};

/**
 * @brief Class representing a list of camera parameters
 */
class CameraParamList {
private:
    ob_camera_param_list_t *impl_ = nullptr;

public:
    CameraParamList(std::unique_ptr<CameraParamListImpl> impl);
    ~CameraParamList() noexcept;

    /**
     * @brief Get the number of camera parameter groups
     *
     * @return uint32_t the number of camera parameter groups
     */
    uint32_t count();

    /**
     * @brief Get the camera parameters for the specified index
     *
     * @param index the index of the parameter group
     * @return OBCameraParam the corresponding group parameters
     */
    OBCameraParam getCameraParam(uint32_t index);
};

/**
 * @brief Class representing a list of OBDepthWorkMode
 */
class OBDepthWorkModeList {
private:
    ob_depth_work_mode_list_t *impl_ = nullptr;

public:
    OBDepthWorkModeList(std::unique_ptr<OBDepthWorkModeListImpl> impl_);
    ~OBDepthWorkModeList();

    /**
     * @brief Get the number of OBDepthWorkMode objects in the list
     *
     * @return uint32_t the number of OBDepthWorkMode objects in the list
     */
    uint32_t count();

    /**
     * @brief Get the OBDepthWorkMode object at the specified index
     *
     * @param index the index of the target OBDepthWorkMode object
     * @return OBDepthWorkMode the OBDepthWorkMode object at the specified index
     */
    OBDepthWorkMode getOBDepthWorkMode(uint32_t index);

    /**
     * @brief Get the name of the depth work mode at the specified index
     *
     * @param index the index of the depth work mode
     * @return const char* the name of the depth work mode
     */
    const char *getName(uint32_t index);

    /**
     * @brief Get the OBDepthWorkMode object at the specified index
     *
     * @param index the index of the target OBDepthWorkMode object
     * @return OBDepthWorkMode the OBDepthWorkMode object at the specified index
     */
    OBDepthWorkMode operator[](uint32_t index);
};

/**
 * @brief Class representing a list of device presets
 * @breif A device preset is a set of parameters or configurations that can be applied to the device to achieve a specific effect or function.
 */
class DevicePresetList {
private:
    ob_device_preset_list_t *impl_ = nullptr;

public:
    DevicePresetList(std::unique_ptr<DevicePresetListImpl> impl);
    ~DevicePresetList() noexcept;

    /**
     * @brief Get the number of device presets in the list
     *
     * @return uint32_t the number of device presets in the list
     */
    uint32_t count();

    /**
     * @brief Get the name of the device preset at the specified index
     *
     * @param index the index of the device preset
     * @return const char* the name of the device preset
     */
    const char *getName(uint32_t index);

    /**
     * @breif check if the preset list contains the special name preset.
     * @param name The name of the preset
     * @return bool Returns true if the special name is found in the preset list, otherwise returns false.
     */
    bool hasPreset(const char *name);
};

}  // namespace ob
