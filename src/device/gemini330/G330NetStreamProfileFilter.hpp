// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

class G330NetStreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    G330NetStreamProfileFilter(IDevice *owner);
    virtual ~G330NetStreamProfileFilter() noexcept = default;

    StreamProfileList filter(const StreamProfileList &profiles) const override;

    void switchFilterMode(OBCameraPerformanceMode mode);

private:
    OBCameraPerformanceMode perFormanceMode_;
};

}  // namespace libobsensor
