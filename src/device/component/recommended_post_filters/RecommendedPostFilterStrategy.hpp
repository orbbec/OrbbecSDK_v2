// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "IFilter.hpp"
#include "IDevice.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace libobsensor {

// Strategy that builds the recommended post-processing filter list for one device family.
// The list (which filters, their order, default parameters and enable state) is the single
// source of truth shared by the live device and the playback device, so both stay in sync.
// Per-filter parameters are pulled from whichever IDepthPostFilterParamsManager the owner has
// registered (device property for live devices, recorded bag for playback) when applicable.
class IRecommendedPostFilterStrategy {
public:
    virtual ~IRecommendedPostFilterStrategy() = default;

    virtual std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) = 0;
};

// Resolves the recommended-filter strategy for a device by its vid/pid.
// Returns nullptr for devices without a dedicated list (callers fall back to a bare threshold filter).
class RecommendedPostFilterStrategyFactory {
public:
    static std::shared_ptr<IRecommendedPostFilterStrategy> create(uint32_t vid, uint32_t pid);
};

}  // namespace libobsensor
