// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "libobsensor/h/ObTypes.h"
#include "libobsensor/h/Context.h"

#include "ImplTypes.hpp"
#include "logger/Logger.hpp"
#include "context/Context.hpp"
#include "environment/EnvConfig.hpp"
#include "shared/utils/Utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_context *ob_create_context(ob_error **error) BEGIN_API_CALL {
    auto ctx      = libobsensor::Context::getInstance();
    auto impl     = new ob_context();
    impl->context = ctx;
    return impl;
}
NO_ARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

ob_context *ob_create_context_with_config(const char *config_path, ob_error **error) BEGIN_API_CALL {
    auto ctx      = libobsensor::Context::getInstance(config_path);
    auto impl     = new ob_context();
    impl->context = ctx;
    return impl;
}
NO_ARGS_HANDLE_EXCEPTIONS_AND_RETURN(nullptr)

void ob_delete_context(ob_context *context, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr = context->context->tryGetDeviceManager();  // Don't use getDeviceManager here
    if(deviceMgr) {
        // stop clock sync if enabled by self
        auto caller = deviceMgr->getDeviceClockSyncCaller();
        if(caller == context) {
            deviceMgr->disableDeviceClockSync();
        }
        // cancel all callbacks
        for(auto id: context->callbackIds) {
            deviceMgr->unregisterDeviceChangedCallback(id);
        }
    }

    delete context;
}
HANDLE_EXCEPTIONS_NO_RETURN(context)

ob_device_list *ob_query_device_list(ob_context *context, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr    = context->context->getDeviceManager();
    auto devListImpl  = new ob_device_list();
    devListImpl->list = deviceMgr->getDeviceInfoList();
    return devListImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, context)

void ob_enable_net_device_enumeration(ob_context *context, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr = context->context->getDeviceManager();
    deviceMgr->enableNetDeviceEnumeration(enable);
}
HANDLE_EXCEPTIONS_NO_RETURN(context, enable)

bool ob_force_ip_config(const char *macAddress, ob_net_ip_config config, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(macAddress);
    if(!libobsensor::utils::checkIpConfig(config)) {
        throw libobsensor::invalid_value_exception("Invalid IP configuration");
        // return false;
    }
    auto ctx    = libobsensor::Context::getInstance();
    auto devMgr = ctx->getDeviceManager();
    return devMgr->forceIpConfig(macAddress, config);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, macAddress)

ob_device *ob_create_net_device(ob_context *context, const char *address, uint16_t port, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr  = context->context->getDeviceManager();
    auto device     = deviceMgr->createNetDevice(address, port, OB_DEVICE_DEFAULT_ACCESS);
    auto devImpl    = new ob_device();
    devImpl->device = device;
    return devImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, context, address, port)

ob_device *ob_create_net_device_ex(ob_context *context, const char *address, uint16_t port, ob_device_access_mode accessMode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    VALIDATE_NOT_EQUAL(accessMode, OB_DEVICE_ACCESS_DENIED);
    auto deviceMgr  = context->context->getDeviceManager();
    auto device     = deviceMgr->createNetDevice(address, port, accessMode);
    auto devImpl    = new ob_device();
    devImpl->device = device;
    return devImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, context, address, port)

void ob_set_device_changed_callback(ob_context *context, ob_device_changed_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    auto id = ob_register_device_changed_callback(context, callback, user_data, error);
    (void)id;
}
HANDLE_EXCEPTIONS_NO_RETURN(context, callback, user_data)

ob_callback_id ob_register_device_changed_callback(ob_context *context, ob_device_changed_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr   = context->context->getDeviceManager();
    auto callback_id = deviceMgr->registerDeviceChangedCallback([callback, user_data](std::vector<std::shared_ptr<const libobsensor::IDeviceEnumInfo>> removed,
                                                                                      std::vector<std::shared_ptr<const libobsensor::IDeviceEnumInfo>> added) {
        auto removedImpl  = new ob_device_list();
        removedImpl->list = removed;
        auto addedImpl    = new ob_device_list();
        addedImpl->list   = added;
        callback(removedImpl, addedImpl, user_data);
    });
    context->callbackIds.push_back(callback_id);
    return callback_id;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, context, callback, user_data)

void ob_unregister_device_changed_callback(ob_context *context, ob_callback_id callback_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr = context->context->getDeviceManager();
    if(deviceMgr->unregisterDeviceChangedCallback(callback_id)) {
        // remove callback id from list
        auto it = std::find_if(context->callbackIds.begin(), context->callbackIds.end(), [callback_id](ob_callback_id id) { return id == callback_id; });
        if(it != context->callbackIds.end()) {
            context->callbackIds.erase(it);
        }
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(context, callback_id)

void ob_enable_device_clock_sync(ob_context *context, uint64_t repeat_interval_msec, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto deviceMgr = context->context->getDeviceManager();
    deviceMgr->enableDeviceClockSync(context, repeat_interval_msec);
}
HANDLE_EXCEPTIONS_NO_RETURN(context, repeat_interval_msec)

void ob_free_idle_memory(ob_context *context, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
    auto frameMemPool = context->context->getFrameMemoryPool();
    frameMemPool->freeIdleMemory();
}
HANDLE_EXCEPTIONS_NO_RETURN(context)

void ob_set_uvc_backend_type(ob_context *context, ob_uvc_backend_type backend_type, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(context);
#if defined(__linux__) || defined(__ANDROID__)
    auto platform = context->context->getPlatform();
    platform->setUvcBackendType(backend_type);
    return;
#else
    libobsensor::utils::unusedVar(backend_type);
    LOG_DEBUG("Set UVC backend type is only available on Linux platforms, ignoring request.");
#endif
}
HANDLE_EXCEPTIONS_NO_RETURN(context, backend_type)

void ob_set_logger_severity(ob_log_severity severity, ob_error **error) BEGIN_API_CALL {
    libobsensor::Logger::setLogSeverity(severity);
}
HANDLE_EXCEPTIONS_NO_RETURN(severity)

void ob_set_logger_to_file(ob_log_severity severity, const char *directory, ob_error **error) BEGIN_API_CALL {
    libobsensor::Logger::setFileLogConfig(severity, directory);
}
HANDLE_EXCEPTIONS_NO_RETURN(severity, directory)

void ob_set_logger_file_name(const char *file_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_STR_NOT_NULL(file_name);
    libobsensor::Logger::setFileLogFileName(file_name);
}
NO_ARGS_HANDLE_EXCEPTIONS_NO_RETURN()

void ob_set_logger_to_callback(ob_log_severity severity, ob_log_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    libobsensor::Logger::setLogCallback(severity, [callback, user_data](ob_log_severity severity, const std::string logMsg) {  //
        callback(severity, logMsg.c_str(), user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(severity, callback, user_data)

void ob_set_logger_to_console(ob_log_severity severity, ob_error **error) BEGIN_API_CALL {
    libobsensor::Logger::setConsoleLogSeverity(severity);
}
HANDLE_EXCEPTIONS_NO_RETURN(severity)

void ob_log_external_message(ob_log_severity severity, const char *module, const char *message, const char *file, const char *func, int line,
                             ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(module);
    VALIDATE_NOT_NULL(message);
    if(file == nullptr) {
        file = "";
    }
    if(func == nullptr) {
        func = "";
    }

    libobsensor::Logger::logExternalMessage(severity, module, message, file, func, line);
}
HANDLE_EXCEPTIONS_NO_RETURN(severity)

void ob_set_extensions_directory(const char *directory, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(directory);
    libobsensor::EnvConfig::setExtensionsDirectory(directory);
}
HANDLE_EXCEPTIONS_NO_RETURN(directory)

#ifdef __cplusplus
}
#endif
