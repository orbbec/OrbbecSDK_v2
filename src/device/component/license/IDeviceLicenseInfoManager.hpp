// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include <string>

namespace libobsensor {

class IDeviceLicenseInfoManager {
public:
    virtual ~IDeviceLicenseInfoManager() = default;

    virtual std::string getLicenseInfo()                              = 0;
    virtual void        writeLicenseInfo(const std::string &jsonInfo) = 0;
    virtual void        clearLicenseInfo()                            = 0;
};

}  // namespace libobsensor
