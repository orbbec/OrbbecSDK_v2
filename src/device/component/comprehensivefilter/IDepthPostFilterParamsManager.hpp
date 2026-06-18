// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "InternalTypes.hpp"
#include <string>
#include <vector>

namespace libobsensor {

// Common interface for depth post-processing filter parameter providers.
// Implemented by both the live-device DepthPostFilterParamsManager (reads the
// raw blob from the device property) and the playback PlaybackDepthPostFilterParamsManager
// (reads it from the recorded bag). Exposing one interface lets the recommended
// filter list be built from a single shared implementation regardless of source.
class IDepthPostFilterParamsManager {
public:
    virtual ~IDepthPostFilterParamsManager() = default;

    virtual FalsePositiveFilterParams *getFPFilterParams()       = 0;
    virtual std::vector<std::string>  &getFPFilterUpdateParams() = 0;
    virtual bool                       isFPFilterEnable()        = 0;

    virtual std::vector<std::string> &getTemporalFilterUpdateParams() = 0;
    virtual bool                      isTemporalFilterEnable()        = 0;

    virtual std::vector<std::string> &getHoleFillingFilterUpdateParams() = 0;
    virtual bool                      isHoleFillingFilterEnable()        = 0;

    virtual std::vector<std::string> &getSpatialFastFilterUpdateParams() = 0;
    virtual bool                      isSpatialFastFilterEnable()        = 0;

    virtual std::vector<std::string> &getSpatialAdvancedFilterUpdateParams() = 0;
    virtual bool                      isSpatialAdvancedFilterEnable()        = 0;

    virtual std::vector<std::string> &getSpatialModerateFilterUpdateParams() = 0;
    virtual bool                      isSpatialModerateFilterEnable()        = 0;

    virtual std::vector<std::string> &getEdgeNoiseRemovalFilterUpdateParams() = 0;
    virtual bool                      isEdgeNoiseRemovalFilterEnable()        = 0;

    virtual std::vector<double> &getNoiseRemovalFilterUpdateParams() = 0;
    virtual bool                 isNoiseRemovalFilterEnable()        = 0;
};

}  // namespace libobsensor
