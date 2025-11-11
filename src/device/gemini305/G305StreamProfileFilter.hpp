// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

class G305StreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    G305StreamProfileFilter(IDevice *owner);
    virtual ~G305StreamProfileFilter() noexcept override = default;

    //StreamProfileList filter(const StreamProfileList &profiles) const override;

    void onPresetResolutionConfigChanged(const OBPresetResolutionConfig *config);

private:
    void fetchPresetResolutionConfig();

private:
    OBPresetResolutionConfig prestResConfig_{};
};

}  // namespace libobsensor
