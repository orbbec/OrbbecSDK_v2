// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

namespace libobsensor {
namespace utils {

double getBagFileVersion();

bool validateBagFileVersion(double recordVersion);

std::string createUnsupportedBagFileVersionMessage(double recordedVersion);
} // namespace utils
}  // namespace libobsensor