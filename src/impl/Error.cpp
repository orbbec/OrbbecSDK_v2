// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Error.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

ob_error *ob_create_error(ob_status status, const char *message, const char *function, const char *args, ob_exception_type exception_type) {
    ob_error *e = new ob_error();
    e->status   = status;

    auto safe_copy = [](char *dest, const char *src, size_t dest_size) {
        if(!src || dest_size == 0) {
            return;
        }
        size_t len = strnlen(src, dest_size - 1);
        memcpy(dest, src, len);
        dest[len] = '\0';
    };

    safe_copy(e->message, message, sizeof(e->message));
    safe_copy(e->function, function, sizeof(e->function));
    safe_copy(e->args, args, sizeof(e->args));
    e->exception_type = exception_type;

    return e;
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
