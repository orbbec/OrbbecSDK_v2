// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {
class OpenNIStreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    OpenNIStreamProfileFilter(IDevice *owner);
    virtual ~OpenNIStreamProfileFilter() noexcept = default;

    StreamProfileList filter(const StreamProfileList &profiles) const override;

private:
    void fetchEffectiveStreamProfiles();

private:
    std::vector<OBEffectiveStreamProfile> colorEffectiveStreamProfiles_;
    std::vector<OBEffectiveStreamProfile> depthEffectiveStreamProfiles_;
};
}  // namespace libobsensor
