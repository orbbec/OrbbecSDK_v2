// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"
#include <iostream>
#include "logger/LoggerTypeHelper.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {
// Overloaded stream operators for custom type serialization and debugging output.
std::ostream &operator<<(std::ostream &os, const libobsensor::DeviceComponentId &id);

}  // namespace libobsensor

// Logger formatter
OB_LOG_FORMATTER(libobsensor::DeviceComponentId)
