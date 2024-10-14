// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/MultipleDevices.h"

#include "ImplTypes.hpp"
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "IDeviceClockSynchronizer.hpp"

#ifdef __cplusplus
extern "C" {
#endif

uint16_t ob_device_get_supported_multi_device_sync_mode_bitmap(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceSyncConfigurator>(libobsensor::OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR);
    return configurator->getSupportedSyncModeBitmap();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void ob_device_set_multi_device_sync_config(ob_device *device, const ob_multi_device_sync_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(config);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceSyncConfigurator>(libobsensor::OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR);
    configurator->setSyncConfig(*config);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

ob_multi_device_sync_config ob_device_get_multi_device_sync_config(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceSyncConfigurator>(libobsensor::OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR);
    return configurator->getSyncConfig();
}
HANDLE_EXCEPTIONS_AND_RETURN({}, device)

void ob_device_trigger_capture(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceSyncConfigurator>(libobsensor::OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR);
    configurator->triggerCapture();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void ob_device_set_timestamp_reset_config(ob_device *device, const ob_device_timestamp_reset_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(config);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceClockSynchronizer>(libobsensor::OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
    configurator->setTimestampResetConfig(*config);
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

ob_device_timestamp_reset_config ob_device_get_timestamp_reset_config(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceClockSynchronizer>(libobsensor::OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
    return configurator->getTimestampResetConfig();
}
HANDLE_EXCEPTIONS_AND_RETURN({}, device)

void ob_device_timestamp_reset(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceClockSynchronizer>(libobsensor::OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
    configurator->timestampReset();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

void ob_device_timer_sync_with_host(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto configurator = device->device->getComponentT<libobsensor::IDeviceClockSynchronizer>(libobsensor::OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
    configurator->timerSyncWithHost();
}
HANDLE_EXCEPTIONS_AND_RETURN(, device)

#ifdef __cplusplus
}
#endif
