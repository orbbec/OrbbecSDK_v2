// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <stdint.h>
#include <algorithm>
#include <set>

#include "IFrame.hpp"
#include "G305MetadataTypes.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "G305FrameTimestampCalculator.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "stream/StreamProfile.hpp"
#include "metadata/FrameMedatadaParser.hpp"

namespace libobsensor {
template <typename T> class G305MetadataTimestampParser : public IFrameMetadataParser {
public:
    G305MetadataTimestampParser(){};
    virtual ~G305MetadataTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md = reinterpret_cast<const T *>(metadata);
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        (void)metadata;
        return dataSize >= sizeof(T);
    }
};

// for depth and ir sensor
class G305MetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G305MetadataSensorTimestampParser(){};
    explicit G305MetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec){};
    virtual ~G305MetadataSensorTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md          = reinterpret_cast<const G305CommonUvcMetadata *>(metadata);
        auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec - exp_in_usec / 2;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        (void)metadata;
        return dataSize >= sizeof(G305CommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G305ColorMetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G305ColorMetadataSensorTimestampParser(){};
    explicit G305ColorMetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec){};
    virtual ~G305ColorMetadataSensorTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md = reinterpret_cast<const G305ColorUvcMetadata *>(metadata);
        // auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec + md->sensor_timestamp_offset_usec;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(G305CommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G305ScrMetadataParserBase : public IFrameMetadataParser {
public:
    G305ScrMetadataParserBase(){};
    virtual ~G305ScrMetadataParserBase() noexcept override = default;

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(StandardUvcFramePayloadHeader::scrSourceClock);
    }
};

class G305PayloadHeadMetadataTimestampParser : public G305ScrMetadataParserBase {
public:
    G305PayloadHeadMetadataTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : device_(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
        timestampCalculator_ = std::make_shared<G305FrameTimestampCalculatorBaseDeviceTime>(device, deviceTimeFreq_, frameTimeFreq_);
    }
    virtual ~G305PayloadHeadMetadataTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto     standardUvcMetadata  = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint64_t presentationTime     = standardUvcMetadata.dwPresentationTime;
        uint64_t transformedTimestamp = ((presentationTime >> 24) & 0xFF) * 1000000 + ((presentationTime & 0xFFFFFF) << 8) / 1000;
        auto     calculatedTimestamp  = timestampCalculator_->calculate(transformedTimestamp);

        return calculatedTimestamp;
    }

private:
    IDevice *device_;

    uint64_t deviceTimeFreq_;

    uint64_t frameTimeFreq_;

    std::shared_ptr<G305FrameTimestampCalculatorBaseDeviceTime> timestampCalculator_;
};

class G305PayloadHeadMetadataColorSensorTimestampParser : public G305PayloadHeadMetadataTimestampParser {
public:
    G305PayloadHeadMetadataColorSensorTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : G305PayloadHeadMetadataTimestampParser(device, deviceTimeFreq, frameTimeFreq) {}
    virtual ~G305PayloadHeadMetadataColorSensorTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color sensor timestamp!");
            return -1;
        }

        auto calculatedTimestamp = G305PayloadHeadMetadataTimestampParser::getValue(metadata, dataSize);
        // get frame offset,unit 100us
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint16_t rawValue            = (((standardUvcMetadata.scrSourceClock[1] & 0xF8) >> 3) | ((standardUvcMetadata.scrSourceClock[2] & 0x7F) << 5));
        int32_t  frameOffset         = 0;
        // 12bit offset value check sign bit
        if(rawValue & 0x800) {
            rawValue    = ((~rawValue) & 0xFFF) + 1;
            frameOffset = (-static_cast<int32_t>(rawValue) * 100);
        }
        else {
            frameOffset = (static_cast<int32_t>(rawValue) * 100);
        }

        calculatedTimestamp += frameOffset;

        return calculatedTimestamp;
    }
};

class G305ColorScrMetadataExposureParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color exposure!");
            return -1;
        }
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint32_t exposure            = (standardUvcMetadata.scrSourceClock[0] | ((standardUvcMetadata.scrSourceClock[1] & 0x07) << 8));

        return static_cast<int64_t>(exposure);
    }
};

class G305ColorScrMetadataActualFrameRateParser : public G305ColorScrMetadataExposureParser {
public:
    G305ColorScrMetadataActualFrameRateParser(IDevice *device) : device_(device) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color actual frame rate!");
            return -1;
        }
        auto exposure = G305ColorScrMetadataExposureParser::getValue(metadata, dataSize) * 100;  // color exposure unit 100us
        auto fps      = static_cast<uint32_t>(1000000.f / exposure);

        auto colorSensor              = device_->getComponentT<VideoSensor>(OB_DEV_COMPONENT_COLOR_SENSOR);
        auto colorActiveStreamProfile = colorSensor->getActivatedStreamProfile();
        if(!colorActiveStreamProfile) {
            return -1;
        }
        auto colorCurrentStreamProfileFps = colorActiveStreamProfile->as<VideoStreamProfile>()->getFps();
        return static_cast<int64_t>((std::min)(fps, colorCurrentStreamProfileFps));
    }

private:
    IDevice *device_;
};

class G305ColorScrMetadataGainParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color gain!");
            return -1;
        }
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint16_t gain                = standardUvcMetadata.scrSourceClock[3];

        return static_cast<int64_t>(gain);
    }
};

class G305PayloadHeadMetadataDepthSensorTimestampParser : public G305PayloadHeadMetadataTimestampParser {
public:
    G305PayloadHeadMetadataDepthSensorTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : G305PayloadHeadMetadataTimestampParser(device, deviceTimeFreq, frameTimeFreq) {}
    virtual ~G305PayloadHeadMetadataDepthSensorTimestampParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color sensor timestamp!");
            return -1;
        }

        auto calculatedTimestamp = G305PayloadHeadMetadataTimestampParser::getValue(metadata, dataSize);
        // get depth exposure,unit 1us
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint32_t exposure =
            standardUvcMetadata.scrSourceClock[0] | (standardUvcMetadata.scrSourceClock[1] << 8) | ((standardUvcMetadata.scrSourceClock[2] & 0x03) << 16);
        calculatedTimestamp = calculatedTimestamp - (exposure / 2);

        return calculatedTimestamp;
    }
};

class G305DepthScrMetadataExposureParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain depth exposure!");
            return -1;
        }
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint32_t exposure =
            standardUvcMetadata.scrSourceClock[0] | (standardUvcMetadata.scrSourceClock[1] << 8) | ((standardUvcMetadata.scrSourceClock[2] & 0x03) << 16);

        return static_cast<int64_t>(exposure);
    }
};

class G305DepthScrMetadataActualFrameRateParser : public G305DepthScrMetadataExposureParser {
public:
    G305DepthScrMetadataActualFrameRateParser(IDevice *device) : device_(device) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain depth actual frame rate!");
            return -1;
        }
        auto exposure = G305DepthScrMetadataExposureParser::getValue(metadata, dataSize);
        auto fps      = static_cast<uint32_t>(1000000.f / exposure);

        static std::vector<DeviceComponentId> sensorComponentIds = { OB_DEV_COMPONENT_DEPTH_SENSOR, OB_DEV_COMPONENT_LEFT_IR_SENSOR,
                                                                     OB_DEV_COMPONENT_RIGHT_IR_SENSOR };

        StreamProfileList streamProfileList;
        uint32_t          activatedStreamProfileFps = 0;
        for(const auto &id: sensorComponentIds) {
            auto sensor                   = device_->getComponentT<VideoSensor>(id);
            auto currentStreamProfileList = sensor->getStreamProfileList();
            auto activeStreamProfile      = sensor->getActivatedStreamProfile();
            if(!activeStreamProfile) {
                continue;
            }
            else {
                streamProfileList         = currentStreamProfileList;
                activatedStreamProfileFps = activeStreamProfile->as<VideoStreamProfile>()->getFps();
                break;
            }
        }

        if(activatedStreamProfileFps == 0) {
            return -1;
        }

        std::vector<uint32_t> currentStreamProfileFpsVector;
        for(const auto &profile: streamProfileList) {
            auto videoStreamProfile = profile->as<VideoStreamProfile>();
            if(!videoStreamProfile) {
                continue;
            }

            auto curFps = videoStreamProfile->getFps();
            if(std::find(currentStreamProfileFpsVector.begin(), currentStreamProfileFpsVector.end(), curFps) != currentStreamProfileFpsVector.end()) {
                continue;
            }

            currentStreamProfileFpsVector.push_back(curFps);
        }
        std::sort(currentStreamProfileFpsVector.begin(), currentStreamProfileFpsVector.end());

        if(fps >= currentStreamProfileFpsVector.back()) {
            fps = currentStreamProfileFpsVector.back();
        }
        else {
            for(size_t i = 0; i < currentStreamProfileFpsVector.size() - 1; i++) {
                if(fps < currentStreamProfileFpsVector[i + 1]) {
                    fps = currentStreamProfileFpsVector[i];
                    break;
                }
            }
        }

        return static_cast<int64_t>((std::min)(fps, activatedStreamProfileFps));
    }

private:
    IDevice *device_;
};

class G305DepthScrMetadataLaserStatusParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain laser status!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint8_t laserStatus         = (standardUvcMetadata.scrSourceClock[2] & 0x04) >> 2;
        return static_cast<int64_t>(laserStatus);
    }
};

class G305DepthScrMetadataLaserPowerLevelParser : public G305ScrMetadataParserBase {
public:
    G305DepthScrMetadataLaserPowerLevelParser(FrameMetadataModifier modifier = nullptr) : modifier_(modifier) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain laser power level!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        int64_t laserPowerLevel     = (standardUvcMetadata.scrSourceClock[2] & 0x38) >> 3;
        if(modifier_) {
            laserPowerLevel = modifier_(laserPowerLevel);
        }
        return laserPowerLevel;
    }

private:
    FrameMetadataModifier modifier_;
};

class G305DepthScrMetadataHDRSequenceIDParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain hdr sequence id!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint8_t hdrSequenceId       = (standardUvcMetadata.scrSourceClock[2] & 0x40) >> 6;
        return static_cast<int64_t>(hdrSequenceId);
    }
};

class G305DepthScrMetadataGainParser : public G305ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain depth gain!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint8_t gain                = standardUvcMetadata.scrSourceClock[3];
        return static_cast<int64_t>(gain);
    }
};

class G305MetadataParserBase : public IFrameMetadataParser {
public:
    G305MetadataParserBase(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier,
                           const std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> metadataTypeIdMap)
        : device_(device), metadataType_(type), data_(0), modifier_(modifier), initPropertyValue_(true) {
        for(const auto &item: metadataTypeIdMap) {
            const std::vector<OBFrameMetadataType> &types = item.second;
            if(std::find(types.begin(), types.end(), type) != types.end()) {
                propertyId_ = item.first;
                break;
            }
        }

        auto propertyServer = device_->getPropertyServer();
        if (propertyServer) {
            propertyServer_ = propertyServer.get();
        }
        if(propertyServer_ && propertyServer_->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            propertyServer_->registerAccessCallback(
                propertyId_, [this, type](uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                    utils::unusedVar(dataSize);
                    utils::unusedVar(operationType);
                    if(propertyId != static_cast<uint32_t>(propertyId_)) {
                        return;
                    }
                    auto propertyItem   = propertyServer_->getPropertyItem(propertyId_, PROP_ACCESS_USER);
                    if(propertyItem.type == OB_STRUCT_PROPERTY) {
                        data_ = parseStructurePropertyValue(type, propertyId, data);
                    }
                    else {
                        data_ = parsePropertyValue(propertyId, data);
                    }
                });
        }
    }

    virtual ~G305MetadataParserBase() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            return -1;
        }

        // first time get value,should sync property value
        if(initPropertyValue_) {
            PropertyAccessType accessType     = PROP_ACCESS_USER;
            auto               propertyServer = device_->getPropertyServer();
            auto               propertyItem   = propertyServer->getPropertyItem(propertyId_, accessType);
            if(propertyItem.type == OB_STRUCT_PROPERTY) {
                auto structValue = propertyServer->getStructureData(propertyId_, accessType);
                data_            = parseStructurePropertyValue(metadataType_, static_cast<uint32_t>(propertyId_), structValue.data());
            }
            else {
                OBPropertyValue value = {};
                propertyServer->getPropertyValue(propertyId_, &value, accessType);
                void *valueData = nullptr;
                if(propertyItem.type == OB_FLOAT_PROPERTY) {
                    valueData = &value.floatValue;
                }
                else {
                    valueData = &value.intValue;
                }
                data_ = parsePropertyValue(static_cast<uint32_t>(propertyId_), (const uint8_t *)valueData);
            }
            registerSensorStateCallback();
            initPropertyValue_ = false;
        }

        if(modifier_) {
            data_ = modifier_(data_);
        }
        return data_;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        utils::unusedVar(dataSize);
        return propertyServer_->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_USER);
    }

private:
    int64_t parsePropertyValue(uint32_t propertyId, const uint8_t *data) {
        int64_t parsedData     = 0;
        auto    value          = reinterpret_cast<const OBPropertyValue *>(data);
        auto    propertyServer = device_->getPropertyServer();
        auto    propertyItem   = propertyServer->getPropertyItem(propertyId, PROP_ACCESS_USER);
        if(propertyItem.type == OB_INT_PROPERTY || propertyItem.type == OB_BOOL_PROPERTY) {
            parsedData = static_cast<int64_t>(value->intValue);
        }
        else if(propertyItem.type == OB_FLOAT_PROPERTY) {
            parsedData = static_cast<int64_t>(value->floatValue);
        }

        return parsedData;
    }

    int64_t parseStructurePropertyValue(OBFrameMetadataType type, uint32_t propertyId, const uint8_t *data) {
        int64_t parsedData = 0;
        if(propertyId == OB_STRUCT_COLOR_AE_ROI || propertyId == OB_STRUCT_DEPTH_AE_ROI) {
            auto roi = *(reinterpret_cast<const OBRegionOfInterest *>(data));
            if(type == OB_FRAME_METADATA_TYPE_AE_ROI_LEFT) {
                parsedData = static_cast<int64_t>(roi.x0_left);
            }
            else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT) {
                parsedData = static_cast<int64_t>(roi.x1_right);
            }
            else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_TOP) {
                parsedData = static_cast<int64_t>(roi.y0_top);
            }
            else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM) {
                parsedData = static_cast<int64_t>(roi.y1_bottom);
            }
        }
        else if(propertyId == OB_STRUCT_DEPTH_HDR_CONFIG) {
            auto hdrConfig = *(reinterpret_cast<const OBHdrConfig *>(data));
            if(type == OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME) {
                parsedData = static_cast<int64_t>(hdrConfig.sequence_name);
            }
            else if(type == OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE) {
                parsedData = static_cast<int64_t>(hdrConfig.enable);
            }
        }

        return parsedData;
    }

    void registerSensorStateCallback() {
        std::vector<DeviceComponentId> compentIds;
        if(propertyId_ == OB_STRUCT_COLOR_AE_ROI && metadataType_ == OB_FRAME_METADATA_TYPE_AE_ROI_LEFT) {
            compentIds.push_back(OB_DEV_COMPONENT_COLOR_SENSOR);
        }
        else if(propertyId_ == OB_STRUCT_DEPTH_AE_ROI && metadataType_ == OB_FRAME_METADATA_TYPE_AE_ROI_LEFT) {
            compentIds.push_back(OB_DEV_COMPONENT_DEPTH_SENSOR);
            compentIds.push_back(OB_DEV_COMPONENT_LEFT_IR_SENSOR);
            compentIds.push_back(OB_DEV_COMPONENT_RIGHT_IR_SENSOR);
        }

        if(compentIds.empty()) {
            return;
        }
        for(auto componentId: compentIds) {
            auto sensor = device_->getComponentT<VideoSensor>(componentId);
            sensor->registerStreamStateChangedCallback([&](OBStreamState state, const std::shared_ptr<const StreamProfile> &profile) {
                utils::unusedVar(profile);
                if(state == STREAM_STATE_STREAMING) {
                    auto propertyServer = device_->getPropertyServer();
                    propertyServer->getStructureDataT<OBRegionOfInterest>(propertyId_);
                }
            });
        }
    }

private:
    IDevice *device_;

    OBFrameMetadataType metadataType_;

    OBPropertyID propertyId_;

    int64_t data_;

    FrameMetadataModifier modifier_;

    std::atomic<bool> initPropertyValue_;
    std::shared_ptr<IPropertyServer> propertyServer_;
};

class G305ColorMetadataParser : public G305MetadataParserBase {
public:
    G305ColorMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G305MetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_COLOR)) {}
    virtual ~G305ColorMetadataParser() override = default;
};

class G305DepthMetadataParser : public G305MetadataParserBase {
public:
    G305DepthMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G305MetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_DEPTH)) {}
    virtual ~G305DepthMetadataParser() override = default;
};

class G305DepthMetadataHdrSequenceSizeParser : public IFrameMetadataParser {
public:
    G305DepthMetadataHdrSequenceSizeParser(IDevice *device) : device_(device), inited_(false), frameInterleaveEnabled_(false), hdrEnabled_(false) {
        auto propertyServer = device_->getPropertyServer();
        if(propertyServer->isPropertySupported(OB_STRUCT_DEPTH_HDR_CONFIG, PROP_OP_WRITE, PROP_ACCESS_USER)) {
            propertyServer->registerAccessCallback(OB_STRUCT_DEPTH_HDR_CONFIG,
                                                   [this](uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                                                       utils::unusedVar(propertyId);
                                                       utils::unusedVar(dataSize);
                                                       utils::unusedVar(operationType);
                                                       auto hdrConfig = *(reinterpret_cast<const OBHdrConfig *>(data));
                                                       hdrEnabled_    = hdrConfig.enable;
                                                       inited_        = true;
                                                   });
        }
        if(propertyServer->isPropertySupported(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, PROP_OP_WRITE, PROP_ACCESS_USER)) {
            propertyServer->registerAccessCallback(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL,
                                                   [this](uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                                                       utils::unusedVar(propertyId);

                                                       utils::unusedVar(dataSize);
                                                       utils::unusedVar(operationType);
                                                       frameInterleaveEnabled_ = *(reinterpret_cast<const bool *>(data));
                                                       inited_                 = true;
                                                   });
        }
    }

    virtual ~G305DepthMetadataHdrSequenceSizeParser() noexcept override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        utils::unusedVar(dataSize);
        if(!inited_) {
            auto propertyServer = device_->getPropertyServer();
            if(propertyServer->isPropertySupported(OB_STRUCT_DEPTH_HDR_CONFIG, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                auto hdrConfig = propertyServer->getStructureDataT<OBHdrConfig>(OB_STRUCT_DEPTH_HDR_CONFIG);
                hdrEnabled_    = hdrConfig.enable;
            }
            if(propertyServer->isPropertySupported(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                frameInterleaveEnabled_ = propertyServer->getPropertyValueT<bool>(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
            }
            inited_ = true;
        }
        return (hdrEnabled_ || frameInterleaveEnabled_) ? 2 : 0;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        utils::unusedVar(dataSize);
        return true;
    }

private:
    IDevice *device_;

    bool inited_;
    bool frameInterleaveEnabled_;
    bool hdrEnabled_;
};

}  // namespace libobsensor
