// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Version.h"

#include "ImplTypes.hpp"
#include "environment/Version.hpp"

#ifdef __cplusplus
extern "C" {
#endif

int ob_get_version() {
    return OB_LIB_VERSION;
}

int ob_get_major_version() {
    return OB_LIB_MAJOR_VERSION;
}

int ob_get_minor_version() {
    return OB_LIB_MINOR_VERSION;
}

int ob_get_patch_version() {
    return OB_LIB_PATCH_VERSION;
}

const char *ob_get_stage_version() {
    return OB_LIB_STAGE_VERSION;
}

#ifdef __cplusplus
}
#endif
