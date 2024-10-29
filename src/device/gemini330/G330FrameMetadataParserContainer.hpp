// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "metadata/FrameMetadataParserContainer.hpp"
#include "G330MetadataTypes.hpp"
#include "G330MetadataParser.hpp"

namespace libobsensor {

class G330ColorFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G330ColorFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330MetadataTimestampParser<G330ColorUvcMetadata>>());
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G330ColorMetadataSensorTimestampParser>([](const int64_t &param) { return param * 100; }));
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G330CommonUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G330ColorUvcMetadata::actual_fps));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G330CommonUvcMetadata::bitmap_union_0,
                                                   [](const int64_t &param) {  //
                                                       return ((G330ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G330CommonUvcMetadata::exposure));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G330ColorUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, makeStructureMetadataParser(&G330ColorUvcMetadata::auto_white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, makeStructureMetadataParser(&G330ColorUvcMetadata::white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, makeStructureMetadataParser(&G330ColorUvcMetadata::brightness));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, makeStructureMetadataParser(&G330ColorUvcMetadata::contrast));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, makeStructureMetadataParser(&G330ColorUvcMetadata::saturation));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, makeStructureMetadataParser(&G330ColorUvcMetadata::sharpness));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION, makeStructureMetadataParser(&G330ColorUvcMetadata::backlight_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, makeStructureMetadataParser(&G330ColorUvcMetadata::gamma));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, makeStructureMetadataParser(&G330ColorUvcMetadata::hue));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY, makeStructureMetadataParser(&G330ColorUvcMetadata::power_line_frequency));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION, makeStructureMetadataParser(&G330ColorUvcMetadata::low_light_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G330ColorUvcMetadata::exposure_roi_bottom));
    }

    virtual ~G330ColorFrameMetadataParserContainer() = default;
};

class G330DepthFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G330DepthFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330MetadataTimestampParser<G330DepthUvcMetadata>>());
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP, std::make_shared<G330MetadataSensorTimestampParser>());
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G330DepthUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G330DepthUvcMetadata::actual_fps));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G330DepthUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G330CommonUvcMetadata::bitmap_union_0,
                                                   [](const uint64_t &param) {  //
                                                       return ((G330ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G330CommonUvcMetadata::exposure));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_priority));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_power));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_power_level));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_STATUS, makeStructureMetadataParser(&G330DepthUvcMetadata::laser_status));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G330DepthUvcMetadata::exposure_roi_bottom));
        registerParser(OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA, makeStructureMetadataParser(&G330DepthUvcMetadata::gpio_input_data));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_name));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_size));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, makeStructureMetadataParser(&G330DepthUvcMetadata::sequence_id));
    }

    virtual ~G330DepthFrameMetadataParserContainer() = default;
};

class G330ColorFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G330ColorFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device = getOwner();

        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330PayloadHeadMetadataTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G330PayloadHeadMetadataColorSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G330ColorScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G330ColorScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G330ColorScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BRIGHTNESS));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_CONTRAST));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SATURATION));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SHARPNESS));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION,
                       std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_GAMMA));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_HUE));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY,
                       std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION,
                       std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY,
                       std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G330ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    };
};

class G330DepthFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G330DepthFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device = getOwner();
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G330PayloadHeadMetadataTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G330PayloadHeadMetadataDepthSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G330DepthScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G330DepthScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G330DepthScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER,
                       std::make_shared<G330DepthScrMetadataLaserPowerLevelParser>([](const int64_t &param) { return param * 80; }));
        registerParser(OB_FRAME_METADATA_TYPE_LASER_POWER_LEVEL, std::make_shared<G330DepthScrMetadataLaserPowerLevelParser>());
        registerParser(OB_FRAME_METADATA_TYPE_LASER_STATUS, std::make_shared<G330DepthScrMetadataLaserStatusParser>());
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, std::make_shared<G330DepthScrMetadataHDRSequenceIDParser>());
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER,
                       std::make_shared<G330DepthScrMetadataHDRSequenceIDParser>());  // todo: remove this line after fix hdr merge issue
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE,
                       std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE,
                                                                 [](const int64_t &param) { return param == 0 ? 0 : 2; }));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G330DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    }
};

}  // namespace libobsensor
