// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

// Activate SIMD path on: ARM NEON, x86 SSSE3+, or MSVC x86-64 (always has SSE4.1 available)
#if defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__) || \
    (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))

#include "UnDistortionImplGeneric.hpp"

#if defined(__ARM_NEON__) || defined(__aarch64__) || defined(__arm__)
#include "SSE2NEON.h"
#else
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#endif

namespace libobsensor {

/**
 * @brief SSE-accelerated undistortion implementation.
 *
 * Inherits LUT build and all format dispatch from UnDistortionImplGeneric.
 * Overrides remapNChannelBilinear to process 4 output pixels per iteration
 * using SSE2/SSE4.1 integer intrinsics for weight computation and
 * weighted-sum reduction.
 */
class UnDistortionImplSSE : public UnDistortionImplGeneric {
public:
    UnDistortionImplSSE()  = default;
    ~UnDistortionImplSSE() override = default;

protected:
    void remapNChannelBilinear(const uint8_t *src, uint8_t *dst,
                               int w, int h, int ch) override;
    void remapYUYV(const uint8_t *src, uint8_t *dst,
                   int w, int h) override;
};

}  // namespace libobsensor

#endif  // __ARM_NEON__ || __NEON__ || __SSSE3__ || MSVC x64
