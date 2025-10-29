// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if !(defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__))

#include "AlignImplGeneric.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

AlignImplGeneric::AlignImplGeneric() {}

AlignImplGeneric::~AlignImplGeneric() {}

void AlignImplGeneric::initialize(OBCameraIntrinsic depth_intrin, OBCameraDistortion depth_disto, OBCameraIntrinsic rgb_intrin, OBCameraDistortion rgb_disto,
                                  OBExtrinsic depth_to_rgb, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy, bool use_scale,
                                  OBFormat depth_format) {
    // TODO
    (void)depth_intrin;
    (void)depth_disto;
    (void)rgb_intrin;
    (void)rgb_disto;
    (void)depth_to_rgb;
    (void)depth_unit_mm;
    (void)add_target_distortion;
    (void)gap_fill_copy;
    (void)use_scale;
    (void)depth_format;
}

float AlignImplGeneric::getDepthUnit() const {
    // TODO
    return 0;
}

void AlignImplGeneric::prepareDepthResolution() {
    // TODO
}

void AlignImplGeneric::reset() {
    // TODO
}

int AlignImplGeneric::D2C(const uint16_t *depth_buffer, int depth_width, int depth_height, uint16_t *out_depth, int color_width, int color_height, int *map,
                          bool withSSE) {
    // TODO
    (void)depth_buffer;
    (void)depth_width;
    (void)depth_height;
    (void)out_depth;
    (void)color_width;
    (void)color_height;
    (void)map;
    (void)withSSE;
    return -1;
}

int AlignImplGeneric::C2D(const uint16_t *depth_buffer, int depth_width, int depth_height, const void *rgb_buffer, void *out_rgb, int color_width,
                          int color_height, OBFormat format, bool withSSE) {
    // TODO
    (void)depth_buffer;
    (void)depth_width;
    (void)depth_height;
    (void)rgb_buffer;
    (void)out_rgb;
    (void)color_width;
    (void)color_height;
    (void)format;
    (void)withSSE;
    return -1;
}

}  // namespace libobsensor

#endif  // __ARM_NEON__ || __NEON__ || __SSSE3__
