// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "JpegUtils.hpp"

#include <logger/Logger.hpp>

namespace libobsensor {
namespace utils {
constexpr size_t JPEG_BOUNDARY_SIZE      = 2;
constexpr size_t JPEG_TRAILING_SCAN_SIZE = 64 * 1024;

JpegStatus checkJpegFrameBoundary(const uint8_t *data, size_t dataLen, size_t *validDataLen) {
    if(data == nullptr || dataLen < JPEG_BOUNDARY_SIZE * 2 || data[0] != 0xFF || data[1] != 0xD8) {
        return JpegStatus::Invalid;
    }

    const size_t searchBegin = dataLen > JPEG_TRAILING_SCAN_SIZE ? dataLen - JPEG_TRAILING_SCAN_SIZE : JPEG_BOUNDARY_SIZE;
    size_t       diagnosticPosition;
    uint8_t      diagnosticBytes[2]{};
    bool         hasTrailingNonZero = false;
    for(size_t index = dataLen; index > searchBegin;) {
        --index;
        if(index > 0 && data[index - 1] == 0xFF && data[index] == 0xD9) {
            if(validDataLen != nullptr) {
                *validDataLen = index + 1;
            }
            return JpegStatus::Confirmed;
        }

        if(!hasTrailingNonZero && data[index] != 0x00) {
            diagnosticPosition = index - 1;
            diagnosticBytes[0] = data[index - 1];
            diagnosticBytes[1] = data[index];
            hasTrailingNonZero = true;
        }
    }

    if(hasTrailingNonZero) {
        LOG_DEBUG("check mjpg end flag failed, data size: {}, search position: {}, bytes: 0x{:02X}, 0x{:02X}", dataLen, diagnosticPosition,
                  static_cast<uint32_t>(diagnosticBytes[0]), static_cast<uint32_t>(diagnosticBytes[1]));
        return JpegStatus::Invalid;
    }

    return JpegStatus::Unconfirmed;
}

int findJpgSequence(const uint8_t *data, uint32_t size, uint32_t startIndex, const uint8_t *target, uint32_t targetLength) {
    // Limit the maximum search length to 1024 bytes for performance and safety
    if(size - startIndex > 1024) {
        size = 1024 + startIndex;
    }

    // Return -1 early if data is too small to contain the target sequence
    if(size < targetLength || startIndex > size - targetLength) {
        return -1;
    }

    const uint32_t maxIndex = size - targetLength;
    for(uint32_t i = startIndex; i <= maxIndex; ++i) {
        bool matched = true;
        for(uint32_t j = 0; j < targetLength; ++j) {
            if(data[i + j] != target[j]) {
                matched = false;
                break;
            }
        }
        if(matched) {
            return i;  // Now returning size_t to match function signature
        }
    }

    return -1;
}

int findJpgSOSSequence(const uint8_t *data, uint32_t size, uint32_t startIndex) {
    constexpr uint8_t kSOSSequence[] = { 0xFF, 0xDA };
    return findJpgSequence(data, size, startIndex, kSOSSequence, sizeof(kSOSSequence));
}

int findJpgCOMSequence(const uint8_t *data, uint32_t size, uint32_t startIndex) {
    constexpr uint8_t kCOMSequence[] = { 0xFF, 0xFE };
    return findJpgSequence(data, size, startIndex, kCOMSequence, sizeof(kCOMSequence));
}

int getJpgHeadLength(const uint8_t *data, uint32_t size) {
    const uint32_t sosSequencefixedDistance = 14;
    int            sosSequenceIndex         = findJpgSOSSequence(data, size);
    if(sosSequenceIndex == -1) {
        return -1;
    }

    uint32_t jpegHeadSize = sosSequenceIndex + sosSequencefixedDistance;
    if(jpegHeadSize >= size) {
        return -1;
    }
    return jpegHeadSize;
}

}  // namespace utils
}  // namespace libobsensor
