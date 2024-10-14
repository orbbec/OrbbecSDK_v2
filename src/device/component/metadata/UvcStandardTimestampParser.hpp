// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "usb/uvc/UvcTypes.hpp"
#include "IFrameMetadataParser.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
class UvcStandardTimestampParser : public IFrameMetadataParser {
public:
    UvcStandardTimestampParser(uint64_t clockFrequency) : clockFrequency_(clockFrequency) {}
    ~UvcStandardTimestampParser() override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            return 0;
        }
        auto header = reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata);
        return static_cast<int64_t>(static_cast<double>(header->dwPresentationTime) / clockFrequency_ * 1000000.0);  // microseconds
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(StandardUvcFramePayloadHeader);
    }

private:
    uint64_t clockFrequency_;
};

// Borrow source clock reference (SCR) as timestamp highest 32 bits
class UvcTimestampParserBorrowScr : public IFrameMetadataParser {
public:
    UvcTimestampParserBorrowScr(uint64_t clockFrequency) : clockFrequency_(clockFrequency) {}
    ~UvcTimestampParserBorrowScr() override = default;

    int64_t getValue(const uint8_t *metadata, size_t dataSize) override {
        if(!isSupported(metadata, dataSize)) {
            return 0;
        }
        auto     header    = reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata);
        uint64_t timestamp = header->dwPresentationTime;

        // the low 4 bytes of scr as the highest 4 bytes of timestamp
        auto scr = header->scrSourceClock;
        timestamp |= static_cast<uint64_t>(scr[0]) << 32;
        timestamp |= static_cast<uint64_t>(scr[1]) << 40;
        timestamp |= static_cast<uint64_t>(scr[2]) << 48;
        timestamp |= static_cast<uint64_t>(scr[3]) << 56;

        return static_cast<int64_t>(static_cast<double>(timestamp) / clockFrequency_ * 1000000.0);  // microseconds
    }

    bool isSupported(const uint8_t *metadata, size_t dataSize) override {
        utils::unusedVar(metadata);
        return dataSize >= sizeof(StandardUvcFramePayloadHeader);
    }

private:
    uint64_t clockFrequency_;
};

}  // namespace libobsensor
