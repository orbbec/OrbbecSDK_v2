// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "metadata/FrameMetadataParserContainer.hpp"
#include "G305MetadataTypes.hpp"
#include "G305MetadataParser.hpp"

namespace libobsensor {

class G305ColorFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G305ColorFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G305MetadataTimestampParser<G305ColorUvcMetadata>>());
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G305ColorMetadataSensorTimestampParser>([](const int64_t &param) { return param * 100; }));
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G305CommonUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G305ColorUvcMetadata::actual_fps));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G305CommonUvcMetadata::bitmap_union_0,
                                                   [](const int64_t &param) {  //
                                                       return ((G305ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G305CommonUvcMetadata::exposure));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G305ColorUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, makeStructureMetadataParser(&G305ColorUvcMetadata::auto_white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, makeStructureMetadataParser(&G305ColorUvcMetadata::white_balance));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, makeStructureMetadataParser(&G305ColorUvcMetadata::brightness));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, makeStructureMetadataParser(&G305ColorUvcMetadata::contrast));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, makeStructureMetadataParser(&G305ColorUvcMetadata::saturation));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, makeStructureMetadataParser(&G305ColorUvcMetadata::sharpness));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION, makeStructureMetadataParser(&G305ColorUvcMetadata::backlight_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, makeStructureMetadataParser(&G305ColorUvcMetadata::gamma));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, makeStructureMetadataParser(&G305ColorUvcMetadata::hue));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY, makeStructureMetadataParser(&G305ColorUvcMetadata::power_line_frequency));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION, makeStructureMetadataParser(&G305ColorUvcMetadata::low_light_compensation));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G305ColorUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G305ColorUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G305ColorUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G305ColorUvcMetadata::exposure_roi_bottom));
    }

    virtual ~G305ColorFrameMetadataParserContainer() override = default;
};

class G305DepthFrameMetadataParserContainer : public FrameMetadataParserContainer {
public:
    G305DepthFrameMetadataParserContainer(IDevice *owner) : FrameMetadataParserContainer(owner) {
        auto device     = getOwner();
        auto propServer = device->getPropertyServer();
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP, std::make_shared<G305MetadataTimestampParser<G305DepthUvcMetadata>>());
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP, std::make_shared<G305MetadataSensorTimestampParser>());
        registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER, makeStructureMetadataParser(&G305DepthUvcMetadata::frame_counter));
        // todo: calculate actual fps according exposure and frame rate
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, makeStructureMetadataParser(&G305DepthUvcMetadata::actual_fps));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, makeStructureMetadataParser(&G305DepthUvcMetadata::gain_level));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE,
                       makeStructureMetadataParser(&G305CommonUvcMetadata::bitmap_union_0,
                                                   [](const uint64_t &param) {  //
                                                       return ((G305ColorUvcMetadata::bitmap_union_0_fields *)&param)->auto_exposure;
                                                   }));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, makeStructureMetadataParser(&G305CommonUvcMetadata::exposure));
        if(propServer->isPropertySupported(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, makeStructureMetadataParser(&G305DepthUvcMetadata::exposure_priority));
        }

        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, makeStructureMetadataParser(&G305DepthUvcMetadata::exposure_roi_left));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, makeStructureMetadataParser(&G305DepthUvcMetadata::exposure_roi_top));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, makeStructureMetadataParser(&G305DepthUvcMetadata::exposure_roi_right));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, makeStructureMetadataParser(&G305DepthUvcMetadata::exposure_roi_bottom));
        registerParser(OB_FRAME_METADATA_TYPE_GPIO_INPUT_DATA, makeStructureMetadataParser(&G305DepthUvcMetadata::gpio_input_data));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME, makeStructureMetadataParser(&G305DepthUvcMetadata::sequence_name));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, makeStructureMetadataParser(&G305DepthUvcMetadata::sequence_size));
        registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, makeStructureMetadataParser(&G305DepthUvcMetadata::sequence_id));
        registerParser(OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_OFFSET, makeStructureMetadataParser(&G305DepthUvcMetadata::disparity_search_offset));
        registerParser(OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE, makeStructureMetadataParser(&G305DepthUvcMetadata::disparity_search_range));
    }

    virtual ~G305DepthFrameMetadataParserContainer() override = default;
};

class G305ColorFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G305ColorFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device = getOwner();

        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorDeviceTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G305ColorScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G305ColorScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G305ColorScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BRIGHTNESS));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_CONTRAST));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SATURATION));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SHARPNESS));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_GAMMA));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_HUE));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    };
};

class G305LeftColorFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G305LeftColorFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device = getOwner();

        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorDeviceTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G305ColorScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G305ColorScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G305LeftColorScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BRIGHTNESS));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_CONTRAST));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SATURATION));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SHARPNESS));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_GAMMA));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_HUE));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    };
};

class G305RightColorFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G305RightColorFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device = getOwner();

        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorRightDeviceTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataColorRightSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G305ColorScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G305ColorScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G305RightColorScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        registerParser(OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_WHITE_BALANCE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_WHITE_BALANCE));
        registerParser(OB_FRAME_METADATA_TYPE_BRIGHTNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BRIGHTNESS));
        registerParser(OB_FRAME_METADATA_TYPE_CONTRAST, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_CONTRAST));
        registerParser(OB_FRAME_METADATA_TYPE_SATURATION, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SATURATION));
        registerParser(OB_FRAME_METADATA_TYPE_SHARPNESS, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_SHARPNESS));
        registerParser(OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_BACKLIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_GAMMA, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_GAMMA));
        registerParser(OB_FRAME_METADATA_TYPE_HUE, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_HUE));
        registerParser(OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_POWER_LINE_FREQUENCY));
        registerParser(OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY,
                       std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_LOW_LIGHT_COMPENSATION));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G305ColorMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    };
};

class G305DepthFrameMetadataParserContainerByScr : public FrameMetadataParserContainer {
public:
    G305DepthFrameMetadataParserContainerByScr(IDevice *owner, const uint64_t deviceTimeFreq, const uint64_t frameTimeFreq)
        : FrameMetadataParserContainer(owner) {
        auto device     = getOwner();
        auto propServer = device->getPropertyServer();
        registerParser(OB_FRAME_METADATA_TYPE_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataDepthDeviceTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_SENSOR_TIMESTAMP,
                       std::make_shared<G305PayloadHeadMetadataDepthSensorTimestampParser>(device, deviceTimeFreq, frameTimeFreq));
        registerParser(OB_FRAME_METADATA_TYPE_GAIN, std::make_shared<G305DepthScrMetadataGainParser>());
        registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE, std::make_shared<G305DepthScrMetadataExposureParser>());
        registerParser(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE, std::make_shared<G305DepthScrMetadataActualFrameRateParser>(device));

        registerParser(OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE, std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AUTO_EXPOSURE));
        if(propServer->isPropertySupported(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            registerParser(OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY,
                           std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_EXPOSURE_PRIORITY));
        }

        registerParser(OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE,
                       std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_DISPARITY_SEARCH_RANGE));
        {
            if(propServer->isPropertySupported(OB_STRUCT_DEPTH_HDR_CONFIG, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                registerParser(OB_FRAME_METADATA_TYPE_FRAME_NUMBER,
                               std::make_shared<G305DepthScrMetadataHDRSequenceIDParser>());  // todo: remove this line after fix hdr merge issue
                registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX, std::make_shared<G305DepthScrMetadataHDRSequenceIDParser>());
                registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME,
                               std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME));
                registerParser(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE, std::make_shared<G305DepthMetadataHdrSequenceSizeParser>(device));
            }
        }

        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_LEFT, std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_LEFT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_TOP, std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_TOP));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT, std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT));
        registerParser(OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM, std::make_shared<G305DepthMetadataParser>(device, OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM));
    }
};

}  // namespace libobsensor
