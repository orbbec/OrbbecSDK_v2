// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ImplTypes.hpp"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static ob_error *ob_create_error_internal(ob_status status, const char *what, const char *name, const char *args, ob_exception_type type) {
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

    safe_copy(e->message, what, sizeof(e->message));
    safe_copy(e->function, name, sizeof(e->function));
    safe_copy(e->args, args, sizeof(e->args));
    e->exception_type = type;

    return e;
}

void translate_exception(const char *name, std::string args, ob_error **result) {
    try {
        throw;
    }
    catch(const libobsensor::libobsensor_exception &e) {
        if(result) {
            *result = ob_create_error_internal(OB_STATUS_ERROR, e.what(), name, args.c_str(), e.get_exception_type());
        }
    }
    catch(const std::exception &e) {
        if(result) {
            *result = ob_create_error_internal(OB_STATUS_ERROR, e.what(), name, args.c_str(), OB_EXCEPTION_STD_EXCEPTION);
        }
    }
    catch(...) {
        if(result) {
            *result = ob_create_error_internal(OB_STATUS_ERROR, "unknown error", name, args.c_str(), OB_EXCEPTION_TYPE_UNKNOWN);
        }
    }
}


#ifdef __cplusplus
}
#endif
