// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {
class MaxProStreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    MaxProStreamProfileFilter(IDevice *owner);
    virtual ~MaxProStreamProfileFilter() noexcept = default;

    StreamProfileList filter(const StreamProfileList &profiles) const override;

private:
    void fetchEffectiveStreamProfiles();

private:
    std::vector<OBEffectiveStreamProfile> effectiveStreamProfiles_;
};
}  // namespace libobsensor
