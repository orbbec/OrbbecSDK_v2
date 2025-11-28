// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

/**
 * @brief LiDARStreamProfileFilter class
 */
class LiDARStreamProfileFilter : public DeviceComponentBase, public IStreamProfileFilter {
public:
    LiDARStreamProfileFilter(IDevice *owner);
    virtual ~LiDARStreamProfileFilter() noexcept override = default;

    StreamProfileList filter(const StreamProfileList &profiles) const override;
};

}  // namespace libobsensor
