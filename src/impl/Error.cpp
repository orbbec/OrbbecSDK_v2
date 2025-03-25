// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Error.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ob_error *ob_create_error(ob_status status, const char *message, const char *function, const char *args, ob_exception_type exception_type) {
    ob_error *error = new ob_error;
    error->status   = status;

    auto message_size = strlen(message) + 1;
    memcpy(error->message, message, message_size > 256 ? 256 : message_size);

    auto function_size = strlen(function) + 1;
    memcpy(error->function, function, function_size > 256 ? 256 : function_size);

    auto args_size = strlen(args) + 1;
    memcpy(error->args, args, args_size > 256 ? 256 : args_size);

    error->exception_type = exception_type;

    return error;
}

ob_status ob_error_get_status(const ob_error *error) {
    return error ? error->status : OB_STATUS_OK;
}

const char *ob_error_get_message(const ob_error *error) {
    return error ? error->message : nullptr;
}

const char *ob_error_get_function(const ob_error *error) {
    return error ? error->function : nullptr;
}

const char *ob_error_get_args(const ob_error *error) {
    return error ? error->args : nullptr;
}

ob_exception_type ob_error_get_exception_type(const ob_error *error) {
    return error ? error->exception_type : OB_EXCEPTION_TYPE_UNKNOWN;
}

void ob_delete_error(ob_error *error) {
    if(error) {
        delete error;
        error = nullptr;
    }
}

#ifdef __cplusplus
}
#endif
