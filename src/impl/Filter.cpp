#include "libobsensor/h/Filter.h"

#include "ImplTypes.hpp"
#include "FilterFactory.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_filter *ob_create_filter(const char *name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(name);
    auto filterFactory = libobsensor::FilterFactory::getInstance();
    auto filter        = filterFactory->createFilter(name);
    auto filterImpl    = new ob_filter();
    filterImpl->filter = filter;
    return filterImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, name)

const char *ob_filter_get_name(const ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    return filter->filter->getName().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, filter)

const char *ob_filter_get_vendor_specific_code(const char *name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(name);
    auto filterFactory        = libobsensor::FilterFactory::getInstance();
    auto filterCreator        = filterFactory->getFilterCreator(name);
    auto privateFilterCreator = std::dynamic_pointer_cast<libobsensor::IPrivFilterCreator>(filterCreator);
    if(privateFilterCreator == nullptr) {
        return nullptr;
    }
    const auto &code = privateFilterCreator->getVendorSpecificCode();
    return code.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, name)

ob_filter *ob_create_private_filter(const char *name, const char *activation_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(name);
    VALIDATE_NOT_NULL(activation_key);
    auto filterFactory = libobsensor::FilterFactory::getInstance();
    auto filter        = filterFactory->createPrivateFilter(name, activation_key);
    auto filterImpl    = new ob_filter();
    filterImpl->filter = filter;
    return filterImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, name, activation_key)

void ob_delete_filter(ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    delete filter;
}
HANDLE_EXCEPTIONS_NO_RETURN(filter)

const char *ob_filter_get_config_schema(const ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    auto &configSchema = filter->filter->getConfigSchema();
    return configSchema.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, filter)

ob_filter_config_schema_list *ob_filter_get_config_schema_list(const ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    auto &configSchemaList = filter->filter->getConfigSchemaVec();
    auto  impl             = new ob_filter_config_schema_list();
    impl->configSchemaList = configSchemaList;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, filter)

uint32_t ob_filter_config_schema_list_get_count(const ob_filter_config_schema_list *config_schema_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config_schema_list);
    return static_cast<uint32_t>(config_schema_list->configSchemaList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, config_schema_list)

ob_filter_config_schema_item ob_filter_config_schema_list_get_item(const ob_filter_config_schema_list *config_schema_list, uint32_t index,
                                                                   ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(config_schema_list);
    VALIDATE_UNSIGNED_INDEX(index,config_schema_list->configSchemaList.size());
    auto &configSchema = config_schema_list->configSchemaList.at(index);
    return configSchema;
}
HANDLE_EXCEPTIONS_AND_RETURN({}, config_schema_list, index)

void ob_filter_update_config(ob_filter *filter, uint8_t argc, const char **argv, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    std::vector<std::string> args(argv, argv + argc);
    filter->filter->updateConfig(args);
}
HANDLE_EXCEPTIONS_NO_RETURN(filter, argc, argv)

double ob_filter_get_config_value(const ob_filter *filter, const char *config_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    VALIDATE_NOT_NULL(config_name);
    return filter->filter->getConfigValue(config_name);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, filter, config_name)

void ob_filter_set_config_value(ob_filter *filter, const char *config_name, double value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    VALIDATE_NOT_NULL(config_name);
    filter->filter->setConfigValue(config_name, value);
}
HANDLE_EXCEPTIONS_NO_RETURN(filter, config_name, value)

void ob_filter_reset(ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    filter->filter->reset();
}
HANDLE_EXCEPTIONS_NO_RETURN(filter)

void ob_filter_enable(ob_filter *filter, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    filter->filter->enable(enable);
}
HANDLE_EXCEPTIONS_NO_RETURN(filter, enable)

bool ob_filter_is_enabled(const ob_filter *filter, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    return filter->filter->isEnabled();
}
HANDLE_EXCEPTIONS_AND_RETURN(false, filter)

ob_frame *ob_filter_process(ob_filter *filter, const ob_frame *frame, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    VALIDATE_NOT_NULL(frame);
    auto result      = filter->filter->process(frame->frame);
    if (result == nullptr) {
        return nullptr;
    }
    auto frameImpl   = new ob_frame();
    frameImpl->frame = result;
    return frameImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, filter, frame)

void ob_filter_set_callback(ob_filter *filter, ob_filter_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    filter->filter->setCallback([callback, user_data](std::shared_ptr<libobsensor::Frame> frame) {
        auto frameImpl   = new ob_frame();
        frameImpl->frame = std::move(frame);
        callback(frameImpl, user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(filter, callback, user_data)

void ob_filter_push_frame(ob_filter *filter, const ob_frame *frame, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter);
    VALIDATE_NOT_NULL(frame);
    filter->filter->pushFrame(frame->frame);
}
HANDLE_EXCEPTIONS_NO_RETURN(filter, frame)

uint32_t ob_filter_list_get_count(const ob_filter_list *filter_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter_list);
    return static_cast<uint32_t>(filter_list->filterList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, filter_list)

ob_filter *ob_filter_list_get_filter(const ob_filter_list *filter_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter_list);
    auto filter        = filter_list->filterList.at(index);
    auto filterImpl    = new ob_filter();
    filterImpl->filter = filter;
    return filterImpl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, filter_list, index)

void ob_delete_filter_list(ob_filter_list *filter_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(filter_list);
    delete filter_list;
}
HANDLE_EXCEPTIONS_NO_RETURN(filter_list)

#ifdef __cplusplus
}
#endif