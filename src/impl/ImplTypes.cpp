// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ImplTypes.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_error *ob_create_error_internal(ob_status status, const char *what, const char *name, const char *args, ob_exception_type type) {
    ob_error *e = new ob_error();
    e->status   = status;
    strcpy(e->message, what);
    strcpy(e->function, name);
    strcpy(e->args, args);
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
