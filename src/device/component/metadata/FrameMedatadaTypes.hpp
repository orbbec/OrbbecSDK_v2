// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "libobsensor/h/Property.h"

#include <map>
#include <vector>

namespace libobsensor {
static const std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> initMetadataTypeIdMap(OBSensorType type) {
    std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> map;

    if(is_color_sensor(type)) {
        map.insert({ OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, { OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE } });
        map.insert({ OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, { OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION } });
        map.insert({ OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, { OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE } });
        map.insert({ OB_PROP_COLOR_WHITE_BALANCE_INT, { OB_FRAME_METADATA_TYPE_WHITE_BALANCE } });
        map.insert({ OB_PROP_COLOR_BRIGHTNESS_INT, { OB_FRAME_METADATA_TYPE_BRIGHTNESS } });
        map.insert({ OB_PROP_COLOR_CONTRAST_INT, { OB_FRAME_METADATA_TYPE_CONTRAST } });
        map.insert({ OB_PROP_COLOR_SATURATION_INT, { OB_FRAME_METADATA_TYPE_SATURATION } });
        map.insert({ OB_PROP_COLOR_SHARPNESS_INT, { OB_FRAME_METADATA_TYPE_SHARPNESS } });
        map.insert({ OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, { OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION } });
        map.insert({ OB_PROP_COLOR_HUE_INT, { OB_FRAME_METADATA_TYPE_HUE } });
        map.insert({ OB_PROP_COLOR_GAMMA_INT, { OB_FRAME_METADATA_TYPE_GAMMA } });
        map.insert({ OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, { OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY } });
        map.insert({ OB_STRUCT_COLOR_AE_ROI,
                     { OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, OB_FRAME_METADATA_TYPE_AE_ROI_TOP, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT,
                       OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM } });
    }
    else if(type == OB_SENSOR_DEPTH) {
        map.insert({ OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, { OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE } });
        map.insert({ OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT, { OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY } });
        map.insert({ OB_STRUCT_DEPTH_HDR_CONFIG, { OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE } });
        map.insert({ OB_STRUCT_DEPTH_AE_ROI,
                     { OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, OB_FRAME_METADATA_TYPE_AE_ROI_TOP, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT,
                       OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM } });
    }

    return map;
}
}  // namespace libobsensor