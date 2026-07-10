// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Advanced.h"

#include "ImplTypes.hpp"
#include "IPresetManager.hpp"
#include "preset/ApplicationConfig.hpp"
#include "exception/ObException.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_application_config *ob_device_get_application_config(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER, false);
    if(!presetMgr || !presetMgr->isApplicationConfigSupported()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("ApplicationConfig is not supported by current device");
    }

    auto config = presetMgr->getApplicationConfig();
    if(!config) {
        return nullptr;
    }

    auto impl    = new ob_application_config();
    impl->config = config;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

ob_application_config *ob_device_get_application_config_by_preset(ob_device *device, const char *preset_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(preset_name);

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER, false);
    if(!presetMgr || !presetMgr->isApplicationConfigSupported()) {
        return nullptr;
    }

    auto config = presetMgr->getApplicationConfig(std::string(preset_name));
    if(!config) {
        return nullptr;
    }

    auto impl    = new ob_application_config();
    impl->config = config;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_delete_application_config(ob_application_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    delete config;
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

bool ob_device_is_application_config_supported(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER, false);
    return presetMgr && presetMgr->isApplicationConfigSupported();
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_application_config_reset(ob_application_config *config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    config->config->reset();
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

void ob_application_config_set_bool(ob_application_config *config, OBAppConfigItem item, bool value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    config->config->setBool(item, value);
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

bool ob_application_config_get_bool(const ob_application_config *config, OBAppConfigItem item, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    return config->config->getBool(item);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, config)

void ob_application_config_set_int(ob_application_config *config, OBAppConfigItem item, int32_t value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    config->config->setInt(item, value);
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

int32_t ob_application_config_get_int(const ob_application_config *config, OBAppConfigItem item, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    return config->config->getInt(item);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, config)

void ob_application_config_set_float(ob_application_config *config, OBAppConfigItem item, float value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    config->config->setFloat(item, value);
}
HANDLE_EXCEPTIONS_NO_RETURN(config)

float ob_application_config_get_float(const ob_application_config *config, OBAppConfigItem item, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    return config->config->getFloat(item);
}
HANDLE_EXCEPTIONS_AND_RETURN(0.0f, config)

void ob_application_config_set_string(ob_application_config *config, OBAppConfigItem item, const char *value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    VALIDATE_STR_NOT_NULL(value);
    config->config->setString(item, value);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, value)

const char *ob_application_config_get_string(const ob_application_config *config, OBAppConfigItem item, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    return config->config->getString(item);
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, config)

void ob_application_config_set_struct(ob_application_config *config, OBAppConfigItem item, const void *value, uint32_t valueSize,
                                      ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    VALIDATE_NOT_NULL(value);
    config->config->setStruct(item, value, valueSize);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, value)

void ob_application_config_get_struct(const ob_application_config *config, OBAppConfigItem item, void *value, uint32_t valueSize,
                                      ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    VALIDATE_NOT_NULL(value);
    VALIDATE_NOT_EQUAL(valueSize, 0);
    config->config->getStruct(item, value, valueSize);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, value)

void ob_application_config_set_profile(ob_application_config *config, OBAppConfigItem item, const ob_stream_profile *profile, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    VALIDATE_NOT_NULL(profile);
    config->config->setProfile(item, profile->profile);
}
HANDLE_EXCEPTIONS_NO_RETURN(config, profile)

ob_stream_profile *ob_application_config_get_profile(const ob_application_config *config, OBAppConfigItem item, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    auto profile = config->config->getProfile(item);
    if(!profile) {
        THROW_INVALID_PARAM_EXCEPTION("Application config profile not found");
    }
    auto impl     = new ob_stream_profile();
    impl->profile = profile;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, config)

uint32_t ob_application_config_get_count(const ob_application_config *config, OBAppConfigKey key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config);
    VALIDATE_NOT_NULL(config->config);
    return config->config->getCount(key);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, config)

#ifdef __cplusplus
}
#endif
