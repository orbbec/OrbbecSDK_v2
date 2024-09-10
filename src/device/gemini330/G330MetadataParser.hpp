#pragma once
#include <stdint.h>
#include <algorithm>

#include "IFrame.hpp"
#include "G330MetadataTypes.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "timestamp/FrameTimestampCalculator.hpp"

namespace libobsensor {

template <typename T, typename Field> class StructureMetadataParser : public IFrameMetadataParser {
public:
    StructureMetadataParser(Field T::*field, FrameMetadataModifier mod) : field_(field), modifier_(mod){};
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

template <typename T> class G330MetadataTimestampParser : public IFrameMetadataParser {
public:
    G330MetadataTimestampParser(){};
    virtual ~G330MetadataTimestampParser() = default;

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
class G330MetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G330MetadataSensorTimestampParser(){};
    explicit G330MetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec){};
    virtual ~G330MetadataSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md          = reinterpret_cast<const G330CommonUvcMetadata *>(metadata);
        auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec - exp_in_usec / 2;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        (void)metadata;
        return dataSize >= sizeof(G330CommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G330ColorMetadataSensorTimestampParser : public IFrameMetadataParser {
public:
    G330ColorMetadataSensorTimestampParser(){};
    explicit G330ColorMetadataSensorTimestampParser(FrameMetadataModifier exp_to_usec) : exp_to_usec_(exp_to_usec){};
    virtual ~G330ColorMetadataSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto md = reinterpret_cast<const G330ColorUvcMetadata *>(metadata);
        // auto exp_in_usec = exp_to_usec_ ? exp_to_usec_(md->exposure) : md->exposure;
        return (int64_t)md->timestamp_sof_sec * 1000000 + md->timestamp_sof_nsec / 1000 - md->timestamp_offset_usec + md->sensor_timestamp_offset_usec;
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(G330CommonUvcMetadata);
    }

private:
    FrameMetadataModifier exp_to_usec_ = nullptr;
};

class G330ScrMetadataParserBase : public IFrameMetadataParser {
public:
    G330ScrMetadataParserBase(){};
    virtual ~G330ScrMetadataParserBase() = default;

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(StandardUvcFramePayloadHeader::scrSourceClock);
    }
};

class G330PayloadHeadMetadataTimestampParser : public G330ScrMetadataParserBase {
public:
    G330PayloadHeadMetadataTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : device_(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
        timestampCalculator_ = std::make_shared<FrameTimestampCalculatorBaseDeviceTime>(device, deviceTimeFreq_, frameTimeFreq_);
    }
    virtual ~G330PayloadHeadMetadataTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain timestamp!");
            return -1;
        }
        auto standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        auto calculatedTimestamp = timestampCalculator_->calculate(static_cast<uint64_t>(standardUvcMetadata.dwPresentationTime));

        return calculatedTimestamp;
    }

private:
    IDevice *device_;

    uint64_t deviceTimeFreq_;

    uint64_t frameTimeFreq_;

    std::shared_ptr<FrameTimestampCalculatorBaseDeviceTime> timestampCalculator_;
};

class G330PayloadHeadMetadataColorSensorTimestampParser : public G330PayloadHeadMetadataTimestampParser {
public:
    G330PayloadHeadMetadataColorSensorTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : G330PayloadHeadMetadataTimestampParser(device, deviceTimeFreq, frameTimeFreq) {}
    virtual ~G330PayloadHeadMetadataColorSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color sensor timestamp!");
            return -1;
        }

        auto calculatedTimestamp = G330PayloadHeadMetadataTimestampParser::getValue(metadata, dataSize);
        // get frame offset,unit 100us
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        int16_t frameOffset         = (((standardUvcMetadata.scrSourceClock[1] & 0xF8) >> 3) | (standardUvcMetadata.scrSourceClock[2] << 8)) * 100;
        calculatedTimestamp += frameOffset;

        return calculatedTimestamp;
    }
};

class G330ColorScrMetadataExposureParser : public G330ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color exposure!");
            return -1;
        }
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint32_t exposure            = (standardUvcMetadata.scrSourceClock[0] | ((standardUvcMetadata.scrSourceClock[1] & 0x07) << 8)) * 100;  // unit 100us

        return static_cast<int64_t>(exposure);
    }
};

class G330ColorScrMetadataActualFrameRateParser : public G330ColorScrMetadataExposureParser {
public:
    G330ColorScrMetadataActualFrameRateParser(IDevice *device) : device_(device) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color actual frame rate!");
            return -1;
        }
        auto exposure = G330ColorScrMetadataExposureParser::getValue(metadata, dataSize);
        auto fps      = static_cast<uint32_t>(1000000.f / exposure);

        auto colorSensor   = device_->getComponentT<VideoSensor>(OB_DEV_COMPONENT_COLOR_SENSOR);
        auto streamProfile = colorSensor->getCurrentBackendStreamProfile();
        if(streamProfile) {
            auto videoStreamProfile = streamProfile->as<VideoStreamProfile>();
            fps                     = std::min(fps, videoStreamProfile->getFps());
        }

        return static_cast<int64_t>(fps);
    }

private:
    IDevice *device_;
};

class G330ColorScrMetadataGainParser : public G330ScrMetadataParserBase {
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

class G330PayloadHeadMetadataDepthSensorTimestampParser : public G330PayloadHeadMetadataTimestampParser {
public:
    G330PayloadHeadMetadataDepthSensorTimestampParser(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
        : G330PayloadHeadMetadataTimestampParser(device, deviceTimeFreq, frameTimeFreq) {}
    virtual ~G330PayloadHeadMetadataDepthSensorTimestampParser() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain color sensor timestamp!");
            return -1;
        }

        auto calculatedTimestamp = G330PayloadHeadMetadataTimestampParser::getValue(metadata, dataSize);
        // get depth exposure,unit 1us
        auto     standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint32_t exposure =
            standardUvcMetadata.scrSourceClock[0] | (standardUvcMetadata.scrSourceClock[1] << 8) | ((standardUvcMetadata.scrSourceClock[2] & 0x03) << 16);
        calculatedTimestamp = calculatedTimestamp - (exposure / 2);

        return calculatedTimestamp;
    }
};

class G330DepthScrMetadataExposureParser : public G330ScrMetadataParserBase {
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

class G330DepthScrMetadataActualFrameRateParser : public G330DepthScrMetadataExposureParser {
public:
    G330DepthScrMetadataActualFrameRateParser(IDevice *device) : device_(device) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain depth actual frame rate!");
            return -1;
        }
        auto exposure = G330DepthScrMetadataExposureParser::getValue(metadata, dataSize);
        auto fps      = static_cast<uint32_t>(1000000.f / exposure);

        auto depthSensor   = device_->getComponentT<VideoSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR);
        auto streamProfile = depthSensor->getCurrentBackendStreamProfile();
        if(streamProfile) {
            auto videoStreamProfile = streamProfile->as<VideoStreamProfile>();
            fps                     = std::min(fps, videoStreamProfile->getFps());
        }

        return static_cast<int64_t>(fps);
    }

private:
    IDevice *device_;
};

class G330DepthScrMetadataLaserStatusParser : public G330ScrMetadataParserBase {
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

class G330DepthScrMetadataLaserPowerLevelParser : public G330ScrMetadataParserBase {
public:
    G330DepthScrMetadataLaserPowerLevelParser(FrameMetadataModifier modifier = nullptr) : modifier_(modifier) {}
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain laser power level!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint8_t laserPowerLevel     = (standardUvcMetadata.scrSourceClock[2] & 0x38) >> 3;
        if(modifier_) {
            laserPowerLevel = static_cast<uint8_t>(modifier_(static_cast<int64_t>(laserPowerLevel)));
        }
        return static_cast<int64_t>(laserPowerLevel);
    }

private:
    FrameMetadataModifier modifier_;
};

class G330DepthScrMetadataHDRSequenceIDParser : public G330ScrMetadataParserBase {
public:
    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            LOG_WARN_INTVL("Current metadata does not contain hdr sequence id!");
            return -1;
        }
        auto    standardUvcMetadata = *(reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata));
        uint8_t hdrSequenceId       = (standardUvcMetadata.scrSourceClock[2] & 0xC0) >> 6;
        return static_cast<int64_t>(hdrSequenceId);
    }
};

class G330DepthScrMetadataGainParser : public G330ScrMetadataParserBase {
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

class G330MetadataParserBase : public IFrameMetadataParser {
public:
    G330MetadataParserBase(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier,
                           const std::multimap<OBPropertyID, std::vector<OBFrameMetadataType>> metadataTypeIdMap)
        : modifier_(modifier) {
        for(const auto &item: metadataTypeIdMap) {
            const std::vector<OBFrameMetadataType> &types = item.second;
            if(std::find(types.begin(), types.end(), type) != types.end()) {
                propertyId_ = item.first;
                break;
            }
        }

        auto propertyServer = device->getPropertyServer();
        propertyServer->registerAccessCallback(propertyId_,
                                               [this, type](uint32_t propertyId, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
                                                   utils::unusedVar(dataSize);
                                                   utils::unusedVar(operationType);
                                                   if(propertyId != static_cast<uint32_t>(propertyId_)) {
                                                       return;
                                                   }
                                                   if(propertyId == OB_STRUCT_COLOR_AE_ROI || propertyId == OB_STRUCT_DEPTH_AE_ROI) {
                                                       auto roi = *(reinterpret_cast<const OBRegionOfInterest *>(data));
                                                       if(type == OB_FRAME_METADATA_TYPE_AE_ROI_LEFT) {
                                                           data_ = static_cast<int64_t>(roi.x0_left);
                                                       }
                                                       else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_RIGHT) {
                                                           data_ = static_cast<int64_t>(roi.x1_right);
                                                       }
                                                       else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_TOP) {
                                                           data_ = static_cast<int64_t>(roi.y0_top);
                                                       }
                                                       else if(type == OB_FRAME_METADATA_TYPE_AE_ROI_BOTTOM) {
                                                           data_ = static_cast<int64_t>(roi.y1_bottom);
                                                       }
                                                   }
                                                   else if(propertyId == OB_STRUCT_DEPTH_HDR_CONFIG) {
                                                       auto hdrConfig = *(reinterpret_cast<const OBHdrConfig *>(data));
                                                       if(type == OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_NAME) {
                                                           data_ = static_cast<int64_t>(hdrConfig.sequence_name);
                                                       }
                                                       else if(type == OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE) {
                                                           data_ = static_cast<int64_t>(hdrConfig.enable);
                                                       }
                                                   }
                                                   else {
                                                       auto value = reinterpret_cast<const OBPropertyValue *>(data);
                                                       // TODO: support float type
                                                       data_ = static_cast<int64_t>(value->intValue);
                                                   }
                                               });
    };

    virtual ~G330MetadataParserBase() = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            return -1;
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
    OBPropertyID propertyId_;

    int64_t data_ = 0;

    FrameMetadataModifier modifier_ = nullptr;
};

class G330ColorMetadataParser : public G330MetadataParserBase {
public:
    G330ColorMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G330MetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_COLOR)) {}
    virtual ~G330ColorMetadataParser() = default;
};

class G330DepthMetadataParser : public G330MetadataParserBase {
public:
    G330DepthMetadataParser(IDevice *device, OBFrameMetadataType type, FrameMetadataModifier modifier = nullptr)
        : G330MetadataParserBase(device, type, modifier, initMetadataTypeIdMap(OB_SENSOR_DEPTH)) {}
    virtual ~G330DepthMetadataParser() = default;
};

}  // namespace libobsensor