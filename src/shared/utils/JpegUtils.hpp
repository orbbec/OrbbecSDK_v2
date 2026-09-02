// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace libobsensor {
namespace utils {

enum class JpegStatus {
    Confirmed,    ///< SOI and EOI boundaries were confirmed.
    Unconfirmed,  ///< SOI was found and the fast tail scan contains only zero padding.
    Invalid,      ///< The data is too short, lacks SOI, or has anomalous trailing data without EOI.
};

/**
 * @brief Checks JPEG SOI/EOI boundaries without decoding the image.
 *
 * @param validDataLen Receives the byte length through EOI when the boundary is confirmed.
 */
JpegStatus checkJpegFrameBoundary(const uint8_t *data, size_t dataLen, size_t *validDataLen = nullptr);

int findJpgSequence(const uint8_t *data, uint32_t size, uint32_t startIndex, const uint8_t *target, uint32_t targetLength);
int findJpgSOSSequence(const uint8_t *data, uint32_t size, uint32_t startIndex = 0);
int findJpgCOMSequence(const uint8_t *data, uint32_t size, uint32_t startIndex = 0);
int getJpgHeadLength(const uint8_t *data, uint32_t size);

}  // namespace utils
}  // namespace libobsensor
