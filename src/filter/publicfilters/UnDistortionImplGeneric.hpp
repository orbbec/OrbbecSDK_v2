// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IUnDistortionImpl.hpp"
#include <vector>

namespace libobsensor {

class UnDistortionImplGeneric : public IUnDistortionImpl {
public:
    UnDistortionImplGeneric()  = default;
    ~UnDistortionImplGeneric() override = default;

    void initialize(const OBCameraIntrinsic  &intrinsic,
                    const OBCameraDistortion  &distortion,
                    const OBCameraIntrinsic   *newCameraIntrinsic,
                    OBFormat                   format,
                    int                        interpMode) override;

    void undistort(const uint8_t *src, uint8_t *dst,
                   int width, int height, OBFormat format) override;

    void reset() override;

protected:
    void buildLUT(const OBCameraIntrinsic  &intrin,
                  const OBCameraDistortion  &disto,
                  const OBCameraIntrinsic   *newCameraIntrin,
                  int outW, int outH);

    // Remap helpers -------------------------------------------------------
    // N-channel uint8 (N = 1/3/4 for Y8, RGB/BGR, RGBA/BGRA).
    // Virtual so the SSE subclass can override only this hot path.
    virtual void remapNChannelBilinear(const uint8_t *src, uint8_t *dst, int w, int h, int ch);
    void remapNChannelNearest (const uint8_t *src, uint8_t *dst, int w, int h, int ch);

    // Single-channel uint16 (Y16 / Z16)
    void remapU16Bilinear(const uint8_t *src, uint8_t *dst, int w, int h);
    void remapU16Nearest (const uint8_t *src, uint8_t *dst, int w, int h);

    // YUYV packed 4:2:2
    virtual void remapYUYV(const uint8_t *src, uint8_t *dst, int w, int h);

    // LUT matching OpenCV's CV_16SC2 + CV_16UC1 format (6 bytes/pixel vs int32×2 = 8 bytes/pixel).
    // mapXi_, mapYi_: integer floor coordinates, clamped to [0, src-1].
    // mapAlpha_: packed 5-bit fractions — (fy5 << 5) | fx5, fy5/fx5 ∈ [0, 31] (INTER_BITS = 5).
    // Bilinear weights: w_tl = (32-fx5)*(32-fy5), sum of all 4 = 1024, result >> 10.
    static constexpr int INTER_BITS = 5;
    static constexpr int INTER_TAB  = 1 << INTER_BITS;   // 32
    static constexpr int INTER_TAB2 = INTER_TAB * INTER_TAB; // 1024
    std::vector<int16_t>  mapXi_;
    std::vector<int16_t>  mapYi_;
    std::vector<uint16_t> mapAlpha_;
    std::vector<int>      oobIndices_;  // pixel indices (v*outW+u) that map outside source bounds
    // Half-width UV LUT for YUYV (kept as int32 fixed-point ×256)
    std::vector<int32_t> mapUVX_;
    std::vector<int32_t> mapUVY_;
    // Pre-computed byte offset into the source YUYV buffer for each output UV
    // pair: src + mapUVOff_[idx] points at the [Y0,U,Y1,V] macro-pixel that
    // contains the chosen U/V samples. Built once per LUT — runtime hot path
    // only does a single int32 load instead of 2×(shift+clamp+mul+div+add).
    std::vector<int32_t> mapUVOff_;

    int      srcW_       = 0;
    int      srcH_       = 0;
    int      outW_       = 0;
    int      outH_       = 0;
    int      interpMode_ = UNDIST_INTERP_BILINEAR;
};

}  // namespace libobsensor
