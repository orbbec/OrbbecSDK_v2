// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "environment/Version.hpp"
#include "MediaUtils.hpp"

#include <cmath> 
#include <map>
#include <string>

namespace libobsensor {
namespace utils {

double getBagFileVersion() {
    constexpr int ver = OB_LIB_VERSION;

    // The entries in versionMap must be sorted from highest to lowest version number
    // so that the loop matches the highest applicable version first.
    // When introducing a new version, insert it in the correct position to maintain this order.
    static const std::pair<int, double> versionMap[] = {
        { 20400, 2.0 },  // ver >= 2.4.0 -> 2.0
        { 0, 1.0 }       // ver < 2.4.0 -> 1.0
    };

    for(auto &p: versionMap) {
        if(ver >= p.first) {
            return p.second;
        }
    }

    return 1.0;
}

bool validateBagFileVersion(double recordedVersion) {
    double currentVersion = getBagFileVersion();

    int currentMajor = static_cast<int>(std::floor(currentVersion));
    int recordMajor = static_cast<int>(std::floor(recordedVersion));

    return currentMajor == recordMajor;
}

std::string createUnsupportedBagFileVersionMessage(double recordedVersion) {
    int recordMajor = static_cast<int>(std::floor(recordedVersion));

    std::string message = "Unsupported bag file version: ";
    if (recordMajor < 2) {
        message += "Please use lower version of OrbbecSDK, or re-record the bag file using a newer version of the SDK.";
    }
    else {
        message += "unknown error.";
    }

    return message;
}
}  // namespace utils
}  // namespace libobsensor