// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include "IStreamProfile.hpp"

#include <memory>

namespace libobsensor {

class ISensorStreamStrategy {
public:
    virtual ~ISensorStreamStrategy() = default;

    virtual void validateStream(const std::shared_ptr<const StreamProfile> &profile)               = 0;
    virtual void validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) = 0;
    virtual void markStreamActivated(const std::shared_ptr<const StreamProfile> &profile)          = 0;
    virtual void markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile)        = 0;
};

}  // namespace libobsensor
