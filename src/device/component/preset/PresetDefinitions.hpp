// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

namespace libobsensor {

/**
 * @brief Json keys for preset
 */

// Common JSON keys
constexpr const char kApiVersion[]    = "api_version";
constexpr const char kFilterNameKey[] = "filter_name";
constexpr const char kEnableKey[]     = "enable";
constexpr const char kLeftKey[]       = "left";
constexpr const char kRightKey[]      = "right";
constexpr const char kTopKey[]        = "top";
constexpr const char kBottomKey[]     = "bottom";

// JSON keys for frame interleave
constexpr const char kInterleaveModeKey[]               = "mode";
constexpr const char kInterleaveConfigIndexKey[]        = "config_index";
constexpr const char kInterleaveParmas[]                = "params";
constexpr const char kInterleaveDepthExposureKey[]      = "depth_exposure";
constexpr const char kInterleaveDepthGainKey[]          = "depth_gain";
constexpr const char kInterleaveDepthBrightnessKey[]    = "depth_brightness";
constexpr const char kInterleaveDepthAeMaxExposureKey[] = "depth_ae_max_exposure";
constexpr const char kInterleaveLaserControlKey[]       = "laser_control";

// JSON keys for software noise removal filter
constexpr const char kNoiseRemovalFilterMinDifferenceKey[] = "min_difference";
constexpr const char kNoiseRemovalFilterMaxSizeKey[]       = "max_size";

// JSON keys for hardware noise removal filter
constexpr const char kNoiseRemovalFilterThresholdKey[] = "threshold";

// JSON keys for disparity search
constexpr const char kDisparitySearchRangeModeKey[] = "range_mode";
constexpr const char kDisparitySearchOffsetKey[]    = "offset";

// JSON keys for stream profile
constexpr const char kStreamProfileFormatKey[] = "format";
constexpr const char kStreamProfileFpsKey[]    = "fps";
constexpr const char kStreamProfileWidthKey[]  = "width";
constexpr const char kStreamProfileHeightKey[] = "height";

}  // namespace libobsensor
