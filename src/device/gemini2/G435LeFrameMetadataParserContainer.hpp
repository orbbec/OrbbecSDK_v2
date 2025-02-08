// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "metadata/FrameMetadataParserContainer.hpp"
#include "G435LeMetadataTypes.hpp"
#include "G435LeMetadataParser.hpp"

namespace libobsensor {

class G435LeColorFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G435LeColorFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        //registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G435LeMetadataTimestampParser<G435LeColorUvcMetadata>>());
        // registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
        //                std::make_shared<G435LeColorMetadataSensorTimestampParser>([](const int64_t &param) { return param * 100; }));
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G435LeCommonUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G435LeColorUvcMetadata::actual_fps));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G435LeCommonUvcMetadata::bitmap_union_0,
                                                   [](const int64_t &param) {  //
                                                       return ((G435LeColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G435LeCommonUvcMetadata::exposure));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G435LeColorUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, makeStructureMetadataParser(&G435LeColorUvcMetadata::auto_white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, makeStructureMetadataParser(&G435LeColorUvcMetadata::white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, makeStructureMetadataParser(&G435LeColorUvcMetadata::brightness));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, makeStructureMetadataParser(&G435LeColorUvcMetadata::contrast));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, makeStructureMetadataParser(&G435LeColorUvcMetadata::saturation));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, makeStructureMetadataParser(&G435LeColorUvcMetadata::sharpness));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION, makeStructureMetadataParser(&G435LeColorUvcMetadata::backlight_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, makeStructureMetadataParser(&G435LeColorUvcMetadata::gamma));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, makeStructureMetadataParser(&G435LeColorUvcMetadata::hue));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY, makeStructureMetadataParser(&G435LeColorUvcMetadata::power_line_frequency));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION, makeStructureMetadataParser(&G435LeColorUvcMetadata::low_light_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G435LeColorUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G435LeColorUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G435LeColorUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G435LeColorUvcMetadata::exposure_roi_bottom));
    }

    virtual ~G435LeColorFrameMetadataParserContainer() = default;
};

class G435LeDepthFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G435LeDepthFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        //registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G435LeMetadataTimestampParser<G435LeDepthUvcMetadata>>());
        // registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP, std::make_shared<G435LeMetadataSensorTimestampParser>());
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G435LeDepthUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G435LeDepthUvcMetadata::actual_fps));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G435LeDepthUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G435LeCommonUvcMetadata::bitmap_union_0,
                                                   [](const uint64_t &param) {  //
                                                       return ((G435LeColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G435LeCommonUvcMetadata::exposure));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, makeStructureMetadataParser(&G435LeDepthUvcMetadata::exposure_priority));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER, makeStructureMetadataParser(&G435LeDepthUvcMetadata::laser_power));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL, makeStructureMetadataParser(&G435LeDepthUvcMetadata::laser_power_level));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_STATUS, makeStructureMetadataParser(&G435LeDepthUvcMetadata::laser_status));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G435LeDepthUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G435LeDepthUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G435LeDepthUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G435LeDepthUvcMetadata::exposure_roi_bottom));
        registerParser(OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA, makeStructureMetadataParser(&G435LeDepthUvcMetadata::gpio_input_data));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, makeStructureMetadataParser(&G435LeDepthUvcMetadata::sequence_name));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, makeStructureMetadataParser(&G435LeDepthUvcMetadata::sequence_size));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, makeStructureMetadataParser(&G435LeDepthUvcMetadata::sequence_id));
    }

    virtual ~G435LeDepthFrameMetadataParserContainer() = default;
};


}  // namespace libobsensor
