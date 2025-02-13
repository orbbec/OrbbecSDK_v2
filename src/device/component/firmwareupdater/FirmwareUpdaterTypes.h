// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/Device.h"
#include "libobsensor/h/Error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pfunc_ob_device_update_firmware_ext)(ob_device *device, const char *path, ob_device_fw_update_callback callback, bool async, void *user_data,
                                                    ob_error **error);

typedef void (*pfunc_ob_device_update_firmware_from_raw_data_ext)(ob_device *device, const uint8_t *firmware_data, uint32_t firmware_size,
                                                                  ob_device_fw_update_callback callback, bool async, void *user_data, ob_error **error);

typedef void (*pfunc_ob_device_optional_depth_presets_ext)(ob_device *device, const char filePathList[][OB_PATH_MAX], uint8_t pathCount,
                                                           ob_device_fw_update_callback callback, void *user_data, ob_error **error);

#ifdef __cplusplus
}
#endif
