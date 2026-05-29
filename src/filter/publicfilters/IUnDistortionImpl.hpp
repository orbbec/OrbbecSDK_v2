// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include <cstdint>

namespace libobsensor {

enum UnDistortionInterpolation {
    UNDIST_INTERP_NEAREST  = 0,
    UNDIST_INTERP_BILINEAR = 1,
};

class IUnDistortionImpl {
public:
    virtual ~IUnDistortionImpl() = default;

    // Build or rebuild the internal LUT.
    // newCameraIntrinsic != nullptr: project output through the new camera matrix (hw_d2c use-case).
    // Otherwise: pure undistortion, same resolution/intrinsic as input.
    virtual void initialize(const OBCameraIntrinsic &intrinsic, const OBCameraDistortion &distortion, const OBCameraIntrinsic *newCameraIntrinsic,
                            OBFormat format, int interpMode) = 0;

    // Remap src -> dst.  Both buffers must cover width*height pixels in the given format.
    virtual void undistort(const uint8_t *src, uint8_t *dst, int width, int height, OBFormat format) = 0;

    virtual void reset() = 0;
};

}  // namespace libobsensor
