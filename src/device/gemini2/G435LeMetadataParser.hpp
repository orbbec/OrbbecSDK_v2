// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <stdint.h>
#include <algorithm>
#include <set>

#include "IFrame.hpp"
#include "G435LeMetadataTypes.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "stream/StreamProfile.hpp"

namespace libobsensor {

template <typename T, typename Field> class StructureMetadataParser : public IFrameMetadataParser {
public:
    StructureMetadataParser(Field T::*field, FrameMetadataModifier mod) : field_(field), modifier_(mod) {};
    virtual ~StructureMetadataParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            throw unsupported_operation_exception("Current metadata does not contain this structure!");
        }
        auto value = static_cast<int64_t>((*reinterpret_cast<const T *>(metadata)).*field_);
        if(modifier_) {
            value = modifier_(value);
        }
        return value;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        (void)metadata;
        return dataSize >= sizeof(T);
    }

private:
    Field T::            *field_;
    FrameMetadataModifier modifier_;
};

template <typename T, typename Field>
std::shared_ptr<StructureMetadataParser<T, Field>> makeStructureMetadataParser(Field T::*field, FrameMetadataModifier mod = nullptr) {
    return std::make_shared<StructureMetadataParser<T, Field>>(field, mod);
}

template <typename T> class G435LeMetadataTimestampParser : public IFrameMetadataParser {
public:
    G435LeMetadataTimestampParser() {};
    virtual ~G435LeMetadataTimestampParser() = default;

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
class G435LeMetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G435LeMetadataSensorTimestampParser() {};
    explicit G435LeMetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec) {};
    virtual ~G435LeMetadataSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md          = reinterpret_cast<const G435LeCommonUvcMetadata *>(metadata);
        auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec - exp_in_usec / 2;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        (void)metadata;
        return dataSize >= sizeof(G435LeCommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G435LeColorMetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G435LeColorMetadataSensorTimestampParser() {};
    explicit G435LeColorMetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec) {};
    virtual ~G435LeColorMetadataSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md = reinterpret_cast<const G435LeColorUvcMetadata *>(metadata);
        // auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec + md->sensor_timestamp_offset_usec;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(G435LeCommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G435LeMetadataParserBase : public IFrameMetadataParser {
public:
    G435LeMetadataParserBase(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier,
                           const std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> metadataTypeIdMap)
        : device_(device), metadataType_(type), data_(0), modifier_(modifier), initPropertyValue_(true) {
        for(const auto &item: metadataTypeIdMap) {
            const std::vector<OBFrameMetadataType> &types = item.second;
            if(std::find(types.begin(), types.end(), type) != types.end()) {
                propertyId_ = item.first;
                break;
            }
        }

        auto propertyServer = device->getPropertyServer();
        if(propertyServer->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            propertyServer->registerAccessCallback(
                propertyId_, [this, type](uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                    utils::unusedVar(dataSize);
                    utils::unusedVar(operationType);
                    if(propertyId != static_cast<uint32_t>(propertyId_)) {
                        return;
                    }
                    auto propertyServer = device_->getPropertyServer();
                    auto propertyItem   = propertyServer->getPropertyItem(propertyId_, PROP_ACCESS_USER);
                    if(propertyItem.type == OB_STRUCT_PROPERTY) {
                        data_ = parseStructurePropertyValue(type, propertyId, data);
                    }
                    else {
                        data_ = parsePropertyValue(propertyId, data);
                    }
                });
        }
    }

    virtual ~G435LeMetadataParserBase() = default;

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
        return true;
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

private:
    IDevice *device_;

    OBFrameMetadataType metadataType_;

    OBPropertyID propertyId_;

    int64_t data_;

    FrameMetadataModifier modifier_;

    std::atomic<bool> initPropertyValue_;
};

class G435LeColorMetadataParser : public G435LeMetadataParserBase {
public:
    G435LeColorMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G435LeMetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_COLOR)) {}
    virtual ~G435LeColorMetadataParser() = default;
};

class G435LeDepthMetadataParser : public G435LeMetadataParserBase {
public:
    G435LeDepthMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G435LeMetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_DEPTH)) {}
    virtual ~G435LeDepthMetadataParser() = default;
};

class G435LeDepthMetadataHdrSequenceSizeParser : public IFrameMetadataParser {
public:
    G435LeDepthMetadataHdrSequenceSizeParser(IDevice *device, FrameMetadataModifier modifier = nullptr)
        : device_(device), modifier_(modifier), inited_(false), frameInterleaveEnabled_(false), hdrEnabled_(false) {
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

    virtual ~G435LeDepthMetadataHdrSequenceSizeParser() = default;

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
    IDevice              *device_;
    FrameMetadataModifier modifier_;

    bool inited_;
    bool frameInterleaveEnabled_;
    bool hdrEnabled_;
};

}  // namespace libobsensor
