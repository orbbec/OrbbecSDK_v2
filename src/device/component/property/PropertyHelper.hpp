// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/Property.h"
#include <map>
#include <string>

namespace libobsensor {
struct OBPropertyBaseInfo {
    OBPropertyType type;
    const char    *name;
};

extern const std::map<uint32_t, OBPropertyBaseInfo> OBPropertyBaseInfoMap;

}  // namespace libobsensor
