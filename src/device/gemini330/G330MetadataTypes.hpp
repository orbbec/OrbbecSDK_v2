// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "usb/uvc/UvcTypes.hpp"
#include "metadata/FrameMedatadaTypes.hpp"

#include <stdint.h>

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
    uint8_t  disparity_search_offset;
    uint8_t  sequence_name;
    uint8_t  sequence_id;
    uint8_t  sequence_size;
    uint8_t  UD0;
    uint8_t  UD1;
    uint8_t  UD2;
    uint8_t  disparity_search_range;
};

#pragma pack(pop)

static_assert(sizeof(G330ColorUvcMetadata) == G330_UVC_METADATA_SIZE, "G330ColorUvcMetadata size mismatch!");
static_assert(sizeof(G330DepthUvcMetadata) == G330_UVC_METADATA_SIZE, "G330DepthUvcMetadata size mismatch!");
}  // namespace libobsensor
