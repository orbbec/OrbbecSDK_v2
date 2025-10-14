// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceTypeHelper.hpp"

namespace libobsensor {

std::ostream &operator<<(std::ostream &os, const libobsensor::DeviceComponentId &id) {
    os << "Device Component '";
    auto iter = libobsensor::DeviceComponentIdMap.find(id);
    if(iter == libobsensor::DeviceComponentIdMap.end()) {
        os << "unknown'";
    }
    else {
        os << iter->second << "'";
    }
    return os;
}

}  // namespace libobsensor
