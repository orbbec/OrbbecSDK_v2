// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {
class G2StreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    G2StreamProfileFilter(IDevice *owner);
    virtual ~G2StreamProfileFilter() noexcept = default;

    StreamProfileList filter(const StreamProfileList &profiles) const override;

private:
    void fetchEffectiveStreamProfiles();

private:
    std::vector<OBEffectiveStreamProfile> effectiveStreamProfiles_;
};
}  // namespace libobsensor
