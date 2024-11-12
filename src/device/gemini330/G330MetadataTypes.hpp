// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <stdint.h>
#include "usb/uvc/UvcTypes.hpp"

namespace libobsensor {

#define G330_UVC_METADATA_SIZE (96 + sizeof(StandardUvcFramePayloadHeader))
#pragma pack(push, 1)
struct G330CommonUvcMetadata {
    StandardUvcFramePayloadHeader uvc_payload_header;
    uint32_t                      frame_counter;
    uint32_t                      timestamp_fsin_h32;
    uint32_t                      timestamp_fsin_l32;
    uint32_t                      timestamp_sof_sec;
    uint32_t                      timestamp_sof_nsec;
    uint32_t                      exposure;
    uint32_t                      timestamp_sync_in_h32;
    uint32_t                      timestamp_sync_in_l32;
    uint32_t                      timestamp_lpps_h32;
    uint32_t                      timestamp_lpps_l32;
    uint32_t                      timestamp_offset_h32;
    uint32_t                      timestamp_offset_l32;
    uint16_t                      digital_gain;
    uint16_t                      analog_gain;

    struct bitmap_union_0_fields {
        uint8_t frame_type : 4;            // depth/disparity/lmono/rmono/YUV422/mjpeg
        uint8_t pixel_available_bits : 4;  // 8bit/10bit/12bit/16bit
        uint8_t padding_mode : 2;          // high bit padding/low bit padding/no padding
        uint8_t auto_exposure : 1;
        uint8_t timestamp_resync_flag : 1;
    };
    uint32_t bitmap_union_0;
    uint32_t timestamp_offset_usec;
};

struct G330ColorUvcMetadata : public G330CommonUvcMetadata {
    uint8_t  low_light_compensation;
    uint8_t  power_line_frequency;
    uint16_t gain_level;
    uint16_t white_balance;
    uint8_t  actual_fps;
    uint8_t  manual_white_balance;
    uint16_t contrast;
    int16_t  brightness;
    uint16_t sharpness;
    uint16_t saturation;
    uint16_t backlight_compensation;
    uint16_t auto_white_balance;
    int16_t  hue;
    uint16_t gamma;
    uint16_t exposure_roi_right;
    uint16_t exposure_roi_left;
    uint16_t exposure_roi_bottom;
    uint16_t exposure_roi_top;
    int32_t  sensor_timestamp_offset_usec;
};

struct G330DepthUvcMetadata : public G330CommonUvcMetadata {
    uint16_t height;
    uint16_t width;
    uint16_t gain_level;
    uint16_t white_balance;
    uint16_t laser_power;
    uint8_t  laser_power_level;
    uint8_t  gpio_input_data;
    uint16_t exposure_roi_right;
    uint16_t exposure_roi_left;
    uint16_t exposure_roi_bottom;
    uint16_t exposure_roi_top;
    uint16_t led_power;
    uint8_t  laser_status;
    uint8_t  exposure_priority;
    uint8_t  actual_fps;
    uint8_t  format;
    uint8_t  sku_id;
    uint8_t  hw_type;
    uint8_t  UD0;
    uint8_t  sequence_name;
    uint8_t  sequence_id;
    uint8_t  sequence_size;
    uint32_t UD1;
};

#pragma pack(pop)

static_assert(sizeof(G330ColorUvcMetadata) == G330_UVC_METADATA_SIZE, "G330ColorUvcMetadata size mismatch!");
static_assert(sizeof(G330DepthUvcMetadata) == G330_UVC_METADATA_SIZE, "G330DepthUvcMetadata size mismatch!");

static const std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> initMetadataTypeIdMap(OBSensorType type) {
    std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> map;

    if(type == OB_SENSOR_COLOR) {
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
        map.insert({ OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, { OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE } });
        map.insert({ OB_STRUCT_DEPTH_AE_ROI,
                     { OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, OB_FRAME_METADATA_TYPE_AE_ROI_TOP, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT,
                       OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM } });
    }

    return map;
}

}  // namespace libobsensor
