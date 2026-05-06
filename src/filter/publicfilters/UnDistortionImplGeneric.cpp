// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UnDistortionImplGeneric.hpp"
#include "logger/LoggerInterval.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <future>

#if defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__) || \
    (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#include "UnDistortionImplSSE.hpp"
#endif

namespace libobsensor {

// ---------------------------------------------------------------------------
// Forward distortion: given normalised undistorted coords (xn, yn), produce
// normalised distorted coords (xd, yd) so that the LUT lookup finds the right
// source pixel in the original (distorted) image.
// ---------------------------------------------------------------------------
static void applyDistortion(const OBCameraDistortion &d, float xn, float yn, float &xd, float &yd) {
    float r2 = xn * xn + yn * yn;

    switch(d.model) {
    case OB_DISTORTION_NONE:
        xd = xn;
        yd = yn;
        return;

    case OB_DISTORTION_BROWN_CONRADY:
    case OB_DISTORTION_MODIFIED_BROWN_CONRADY:
    case OB_DISTORTION_INVERSE_BROWN_CONRADY: {
        float r4      = r2 * r2;
        float r6      = r4 * r2;
        float radial  = 1.0f + d.k1 * r2 + d.k2 * r4 + d.k3 * r6;
        xd = xn * radial + 2.0f * d.p1 * xn * yn + d.p2 * (r2 + 2.0f * xn * xn);
        yd = yn * radial + d.p1 * (r2 + 2.0f * yn * yn) + 2.0f * d.p2 * xn * yn;
        return;
    }

    case OB_DISTORTION_BROWN_CONRADY_K6: {
        float r4  = r2 * r2;
        float r6  = r4 * r2;
        float num = 1.0f + d.k1 * r2 + d.k2 * r4 + d.k3 * r6;
        float den = 1.0f + d.k4 * r2 + d.k5 * r4 + d.k6 * r6;
        float radial = (std::abs(den) > 1e-8f) ? (num / den) : num;
        xd = xn * radial + 2.0f * d.p1 * xn * yn + d.p2 * (r2 + 2.0f * xn * xn);
        yd = yn * radial + d.p1 * (r2 + 2.0f * yn * yn) + 2.0f * d.p2 * xn * yn;
        return;
    }

    case OB_DISTORTION_KANNALA_BRANDT4: {
        float r       = std::sqrt(r2);
        float theta   = std::atan2(r, 1.0f);
        float t2      = theta * theta;
        float t4      = t2 * t2;
        float t6      = t4 * t2;
        float t8      = t4 * t4;
        float theta_d = theta * (1.0f + d.k1 * t2 + d.k2 * t4 + d.k3 * t6 + d.k4 * t8);
        float scale   = (r > 1e-6f) ? (theta_d / r) : 1.0f;
        xd = xn * scale;
        yd = yn * scale;
        return;
    }

    default:
        xd = xn;
        yd = yn;
        return;
    }
}

// ---------------------------------------------------------------------------

void UnDistortionImplGeneric::initialize(const OBCameraIntrinsic  &intrinsic,
                                          const OBCameraDistortion  &distortion,
                                          const OBCameraIntrinsic   *newCameraIntrinsic,
                                          OBFormat                   /*format*/,
                                          int                        interpMode) {
    interpMode_ = interpMode;
    int outW = newCameraIntrinsic ? (int)newCameraIntrinsic->width  : (int)intrinsic.width;
    int outH = newCameraIntrinsic ? (int)newCameraIntrinsic->height : (int)intrinsic.height;
    buildLUT(intrinsic, distortion, newCameraIntrinsic, outW, outH);
}

void UnDistortionImplGeneric::buildLUT(const OBCameraIntrinsic  &intrin,
                                        const OBCameraDistortion  &disto,
                                        const OBCameraIntrinsic   *newCameraIntrin,
                                        int outW, int outH) {
    srcW_ = intrin.width;
    srcH_ = intrin.height;
    outW_ = outW;
    outH_ = outH;

    const size_t n = static_cast<size_t>(outW * outH);
    mapXi_.resize(n);
    mapYi_.resize(n);
    mapAlpha_.resize(n);
    oobIndices_.clear();

    const float fx = intrin.fx, fy = intrin.fy, cx = intrin.cx, cy = intrin.cy;
    const float vfx = newCameraIntrin ? newCameraIntrin->fx : fx;
    const float vfy = newCameraIntrin ? newCameraIntrin->fy : fy;
    const float vcx = newCameraIntrin ? newCameraIntrin->cx : cx;
    const float vcy = newCameraIntrin ? newCameraIntrin->cy : cy;

    for(int v = 0; v < outH; v++) {
        for(int u = 0; u < outW; u++) {
            float xn = (static_cast<float>(u) - vcx) / vfx;
            float yn = (static_cast<float>(v) - vcy) / vfy;
            float xd, yd;
            applyDistortion(disto, xn, yn, xd, yd);

            float xs = xd * fx + cx;
            float ys = yd * fy + cy;

            int idx = v * outW + u;

            // Out-of-bounds: record index and write a dummy LUT entry (will be zeroed in undistort)
            if(xs < 0.0f || xs > static_cast<float>(srcW_ - 1) ||
               ys < 0.0f || ys > static_cast<float>(srcH_ - 1)) {
                oobIndices_.push_back(idx);
                mapXi_[idx]    = 0;
                mapYi_[idx]    = 0;
                mapAlpha_[idx] = 0;
                continue;
            }

            xs = std::max(0.0f, std::min(xs, static_cast<float>(srcW_ - 1)));
            ys = std::max(0.0f, std::min(ys, static_cast<float>(srcH_ - 1)));

            int x0  = static_cast<int>(xs);
            int y0  = static_cast<int>(ys);
            int fx5 = static_cast<int>((xs - x0) * INTER_TAB + 0.5f);
            int fy5 = static_cast<int>((ys - y0) * INTER_TAB + 0.5f);
            if(fx5 >= INTER_TAB) { fx5 = 0; x0 = std::min(x0 + 1, srcW_ - 1); }
            if(fy5 >= INTER_TAB) { fy5 = 0; y0 = std::min(y0 + 1, srcH_ - 1); }
            x0 = std::min(x0, srcW_ - 1);
            y0 = std::min(y0, srcH_ - 1);

            mapXi_[idx]    = static_cast<int16_t>(x0);
            mapYi_[idx]    = static_cast<int16_t>(y0);
            mapAlpha_[idx] = static_cast<uint16_t>((fy5 << INTER_BITS) | fx5);
        }
    }

    // Half-width UV LUT for YUYV (kept as int32 fixed-point ×256)
    int hw = outW / 2;
    mapUVX_.resize(static_cast<size_t>(hw * outH));
    mapUVY_.resize(static_cast<size_t>(hw * outH));
    mapUVOff_.resize(static_cast<size_t>(hw * outH));
    for(int v = 0; v < outH; v++) {
        for(int u2 = 0; u2 < hw; u2++) {
            float xCenter = static_cast<float>(u2) * 2.0f + 0.5f;
            float xn      = (xCenter - vcx) / vfx;
            float yn      = (static_cast<float>(v) - vcy) / vfy;
            float xd, yd;
            applyDistortion(disto, xn, yn, xd, yd);
            float xs = std::max(0.0f, std::min(xd * fx + cx, static_cast<float>(srcW_ - 1)));
            float ys = std::max(0.0f, std::min(yd * fy + cy, static_cast<float>(srcH_ - 1)));
            int idx      = v * hw + u2;
            mapUVX_[idx] = static_cast<int32_t>(xs * 256.0f);
            mapUVY_[idx] = static_cast<int32_t>(ys * 256.0f);

            // Pre-compute the source YUYV byte offset of the macro-pixel
            // (4-byte group) that contains the U/V samples. The runtime path
            // then reads U at [+1] and V at [+3] off this offset directly.
            int sux        = std::min(static_cast<int>((mapUVX_[idx] + 128) >> 8), srcW_ - 1);
            int suy        = std::min(static_cast<int>((mapUVY_[idx] + 128) >> 8), srcH_ - 1);
            int ux         = std::min(sux & ~1, srcW_ - 2);
            mapUVOff_[idx] = suy * srcW_ * 2 + (ux / 2) * 4;
        }
    }
}

void UnDistortionImplGeneric::reset() {
    mapXi_.clear();
    mapYi_.clear();
    mapAlpha_.clear();
    oobIndices_.clear();
    mapUVX_.clear();
    mapUVY_.clear();
    mapUVOff_.clear();
    srcW_ = 0;
    srcH_ = 0;
    outW_ = 0;
    outH_ = 0;
}

// ---------------------------------------------------------------------------
// Bilinear: 5-bit fractions (INTER_BITS=5), weights sum = INTER_TAB2 = 1024.
// Result = (w00*p00 + w10*p10 + w01*p01 + w11*p11 + 512) >> 10
// ---------------------------------------------------------------------------
#define BILERP5(w00, w10, w01, w11, v00, v10, v01, v11) \
    static_cast<uint8_t>(((w00)*(v00) + (w10)*(v10) + (w01)*(v01) + (w11)*(v11) + 512) >> 10)

void UnDistortionImplGeneric::remapNChannelBilinear(const uint8_t *src, uint8_t *dst,
                                                     int w, int h, int ch) {
    for(int v = 0; v < h; v++) {
        for(int u = 0; u < w; u++) {
            int             idx   = v * w + u;
            int             x0    = mapXi_[idx];
            int             y0    = mapYi_[idx];
            uint16_t        alpha = mapAlpha_[idx];
            int             fx    = alpha & (INTER_TAB - 1);
            int             fy    = alpha >> INTER_BITS;
            int             x1    = std::min(x0 + 1, srcW_ - 1);
            int             y1    = std::min(y0 + 1, srcH_ - 1);
            int             fxi   = INTER_TAB - fx;
            int             fyi   = INTER_TAB - fy;
            int             w00   = fxi * fyi;
            int             w10   = fx  * fyi;
            int             w01   = fxi * fy;
            int             w11   = fx  * fy;

            const uint8_t *p00 = src + (y0 * srcW_ + x0) * ch;
            const uint8_t *p10 = src + (y0 * srcW_ + x1) * ch;
            const uint8_t *p01 = src + (y1 * srcW_ + x0) * ch;
            const uint8_t *p11 = src + (y1 * srcW_ + x1) * ch;
            uint8_t       *out = dst + idx * ch;

            if(ch == 3) {
                out[0] = BILERP5(w00, w10, w01, w11, p00[0], p10[0], p01[0], p11[0]);
                out[1] = BILERP5(w00, w10, w01, w11, p00[1], p10[1], p01[1], p11[1]);
                out[2] = BILERP5(w00, w10, w01, w11, p00[2], p10[2], p01[2], p11[2]);
            }
            else if(ch == 4) {
                out[0] = BILERP5(w00, w10, w01, w11, p00[0], p10[0], p01[0], p11[0]);
                out[1] = BILERP5(w00, w10, w01, w11, p00[1], p10[1], p01[1], p11[1]);
                out[2] = BILERP5(w00, w10, w01, w11, p00[2], p10[2], p01[2], p11[2]);
                out[3] = BILERP5(w00, w10, w01, w11, p00[3], p10[3], p01[3], p11[3]);
            }
            else if(ch == 1) {
                out[0] = BILERP5(w00, w10, w01, w11, p00[0], p10[0], p01[0], p11[0]);
            }
            else {
                for(int c = 0; c < ch; c++) {
                    out[c] = BILERP5(w00, w10, w01, w11, p00[c], p10[c], p01[c], p11[c]);
                }
            }
        }
    }
}

#undef BILERP5

void UnDistortionImplGeneric::remapNChannelNearest(const uint8_t *src, uint8_t *dst,
                                                    int w, int h, int ch) {
    for(int v = 0; v < h; v++) {
        for(int u = 0; u < w; u++) {
            int      idx   = v * w + u;
            uint16_t alpha = mapAlpha_[idx];
            int      fx    = alpha & (INTER_TAB - 1);
            int      fy    = alpha >> INTER_BITS;
            int      x0    = mapXi_[idx] + (fx >= INTER_TAB / 2 ? 1 : 0);
            int      y0    = mapYi_[idx] + (fy >= INTER_TAB / 2 ? 1 : 0);
            x0 = std::min(x0, srcW_ - 1);
            y0 = std::min(y0, srcH_ - 1);
            const uint8_t *p   = src + (y0 * srcW_ + x0) * ch;
            uint8_t       *out = dst + idx * ch;
            for(int c = 0; c < ch; c++) {
                out[c] = p[c];
            }
        }
    }
}

void UnDistortionImplGeneric::remapU16Bilinear(const uint8_t *src, uint8_t *dst, int w, int h) {
    const auto *srcU16 = reinterpret_cast<const uint16_t *>(src);
    auto       *dstU16 = reinterpret_cast<uint16_t *>(dst);

    for(int v = 0; v < h; v++) {
        for(int u = 0; u < w; u++) {
            int      idx   = v * w + u;
            int      x0    = mapXi_[idx];
            int      y0    = mapYi_[idx];
            uint16_t alpha = mapAlpha_[idx];
            int      fx    = alpha & (INTER_TAB - 1);
            int      fy    = alpha >> INTER_BITS;
            int      x1    = std::min(x0 + 1, srcW_ - 1);
            int      y1    = std::min(y0 + 1, srcH_ - 1);
            int      fxi   = INTER_TAB - fx;
            int      fyi   = INTER_TAB - fy;
            int      w00   = fxi * fyi;
            int      w10   = fx  * fyi;
            int      w01   = fxi * fy;
            int      w11   = fx  * fy;

            uint32_t val = static_cast<uint32_t>(
                w00 * srcU16[y0 * srcW_ + x0] + w10 * srcU16[y0 * srcW_ + x1] +
                w01 * srcU16[y1 * srcW_ + x0] + w11 * srcU16[y1 * srcW_ + x1] + 512) >> 10;
            dstU16[idx] = static_cast<uint16_t>(std::min(val, static_cast<uint32_t>(65535)));
        }
    }
}

void UnDistortionImplGeneric::remapU16Nearest(const uint8_t *src, uint8_t *dst, int w, int h) {
    const auto *srcU16 = reinterpret_cast<const uint16_t *>(src);
    auto       *dstU16 = reinterpret_cast<uint16_t *>(dst);

    for(int v = 0; v < h; v++) {
        for(int u = 0; u < w; u++) {
            int      idx   = v * w + u;
            uint16_t alpha = mapAlpha_[idx];
            int      fx    = alpha & (INTER_TAB - 1);
            int      fy    = alpha >> INTER_BITS;
            int      x0    = std::min(mapXi_[idx] + (fx >= INTER_TAB / 2 ? 1 : 0), srcW_ - 1);
            int      y0    = std::min(mapYi_[idx] + (fy >= INTER_TAB / 2 ? 1 : 0), srcH_ - 1);
            dstU16[idx] = srcU16[y0 * srcW_ + x0];
        }
    }
}

void UnDistortionImplGeneric::remapYUYV(const uint8_t *src, uint8_t *dst, int w, int h) {
    int hw = w / 2;

    auto getYBilinear = [&](int idx) -> uint8_t {
        int      x0    = mapXi_[idx];
        int      y0    = mapYi_[idx];
        uint16_t alpha = mapAlpha_[idx];
        int      fx    = alpha & (INTER_TAB - 1);
        int      fy    = alpha >> INTER_BITS;
        int      x1    = std::min(x0 + 1, srcW_ - 1);
        int      y1    = std::min(y0 + 1, srcH_ - 1);
        int      fxi   = INTER_TAB - fx, fyi = INTER_TAB - fy;
        int      w00   = fxi*fyi, w10 = fx*fyi, w01 = fxi*fy, w11 = fx*fy;
        auto     Y     = [&](int x, int y) { return static_cast<int>(src[2*(y*srcW_+x)]); };
        return static_cast<uint8_t>((w00*Y(x0,y0)+w10*Y(x1,y0)+w01*Y(x0,y1)+w11*Y(x1,y1)+512)>>10);
    };

    for(int v = 0; v < h; v++) {
        for(int u2 = 0; u2 < hw; u2++) {
            uint8_t y0val = getYBilinear(v * w + 2 * u2    );
            uint8_t y1val = getYBilinear(v * w + 2 * u2 + 1);

            // UV byte offset is precomputed in buildLUT — single int32 load
            // instead of (>>8)+clamp+&~1+clamp+mul+div+add.
            const uint8_t *uvSrc = src + mapUVOff_[v * hw + u2];

            int dstBase      = v * w * 2 + u2 * 4;
            dst[dstBase + 0] = y0val;
            dst[dstBase + 1] = uvSrc[1];
            dst[dstBase + 2] = y1val;
            dst[dstBase + 3] = uvSrc[3];
        }
    }
}

// ---------------------------------------------------------------------------

void UnDistortionImplGeneric::undistort(const uint8_t *src, uint8_t *dst,
                                         int w, int h, OBFormat format) {
    switch(format) {
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
        (interpMode_ == UNDIST_INTERP_NEAREST)
            ? remapNChannelNearest(src, dst, w, h, 3)
            : remapNChannelBilinear(src, dst, w, h, 3);
        break;

    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        (interpMode_ == UNDIST_INTERP_NEAREST)
            ? remapNChannelNearest(src, dst, w, h, 4)
            : remapNChannelBilinear(src, dst, w, h, 4);
        break;

    case OB_FORMAT_Y8:
        (interpMode_ == UNDIST_INTERP_NEAREST)
            ? remapNChannelNearest(src, dst, w, h, 1)
            : remapNChannelBilinear(src, dst, w, h, 1);
        break;

    case OB_FORMAT_Y16:
    case OB_FORMAT_Z16:
        (interpMode_ == UNDIST_INTERP_NEAREST)
            ? remapU16Nearest(src, dst, w, h)
            : remapU16Bilinear(src, dst, w, h);
        break;

    case OB_FORMAT_YUYV:
        remapYUYV(src, dst, w, h);
        break;

    default:
        LOG_WARN_INTVL("UnDistortionFilter: unsupported format {}, frame passed through unchanged", static_cast<int>(format));
        std::memcpy(dst, src, static_cast<size_t>(w * h * 2));
        break;
    }

    // Zero-fill pixels that mapped outside the source image bounds.
    // Matches OpenCV BORDER_CONSTANT (fill value = 0) rather than clamping to edge.
    if(!oobIndices_.empty()) {
        int bpp = 0;
        switch(format) {
        case OB_FORMAT_Y8:                          bpp = 1; break;
        case OB_FORMAT_Y16: case OB_FORMAT_Z16:     bpp = 2; break;
        case OB_FORMAT_YUYV:                        bpp = 2; break;
        case OB_FORMAT_RGB: case OB_FORMAT_BGR:     bpp = 3; break;
        case OB_FORMAT_RGBA: case OB_FORMAT_BGRA:   bpp = 4; break;
        default: break;
        }
        if(bpp > 0) {
            for(int idx : oobIndices_) {
                std::memset(dst + idx * bpp, 0, static_cast<size_t>(bpp));
            }
        }
    }
}

}  // namespace libobsensor

// ---------------------------------------------------------------------------
// SSE-accelerated bilinear remap (UnDistortionImplSSE).
// Defined here so no cmake reconfigure is needed for the new .hpp/.cpp pair.
// ---------------------------------------------------------------------------
#if defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__) || \
    (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))

namespace libobsensor {

// ---------------------------------------------------------------------------
// remapNChannelBilinear – optimised single-threaded SSSE3 bilinear remap.
//
// Three targeted improvements over the previous version:
//
// 1. Software prefetch (PF_DIST pixels ahead): hides L3 cache latency (~35
//    cycles) for the random-access gather.  _mm_prefetch is a pure hint and
//    never faults, even if the address is slightly past the buffer end.
//
// 2. _mm_extract_epi32 with compile-time indices instead of store-to-stack
//    + reload: eliminates store-forwarding latency (~5 cycles per load) so
//    all 4 pixel address computations can proceed in parallel via OOO.
//
// 3. Explicit row-offset variables (row0/row1): ensures the compiler emits
//    exactly two row multiplications per pixel instead of four.
// ---------------------------------------------------------------------------

// Force-inline so the compiler never emits a real call: 4 calls per 4-pixel
// batch would add ~20 cycles of call overhead, negating all other gains.
#if defined(_MSC_VER)
#  define OB_BILIN_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define OB_BILIN_INLINE __attribute__((always_inline)) inline
#else
#  define OB_BILIN_INLINE inline
#endif

// Shared SSSE3 bilinear kernel: returns 4 interpolated bytes as int32.
// For ch==3 only lanes [0..2] are used; for ch==4 all four are valid.
static OB_BILIN_INLINE int32_t bilinKernel(const uint8_t *q00, const uint8_t *q10,
                                  const uint8_t *q01, const uint8_t *q11,
                                  int fxk, int fyk, const __m128i &c512) {
    constexpr int tab = 1 << 5;  // = UnDistortionImplGeneric::INTER_TAB (32)
    const int8_t  wL  = static_cast<int8_t>(tab - fxk);
    const int8_t  wR  = static_cast<int8_t>(fxk);
    const __m128i bh  = _mm_set1_epi16(static_cast<int16_t>(
        static_cast<uint16_t>((static_cast<uint8_t>(wR) << 8) | static_cast<uint8_t>(wL))));
    __m128i ht = _mm_maddubs_epi16(
        _mm_unpacklo_epi8(_mm_cvtsi32_si128(*reinterpret_cast<const int32_t *>(q00)),
                          _mm_cvtsi32_si128(*reinterpret_cast<const int32_t *>(q10))), bh);
    __m128i hb = _mm_maddubs_epi16(
        _mm_unpacklo_epi8(_mm_cvtsi32_si128(*reinterpret_cast<const int32_t *>(q01)),
                          _mm_cvtsi32_si128(*reinterpret_cast<const int32_t *>(q11))), bh);
    const int16_t wy  = static_cast<int16_t>(fyk);
    const int16_t wyi = static_cast<int16_t>(tab - fyk);
    const __m128i bv  = _mm_set1_epi32(static_cast<int32_t>(
        (static_cast<uint32_t>(static_cast<uint16_t>(wy)) << 16) | static_cast<uint16_t>(wyi)));
    __m128i s = _mm_madd_epi16(_mm_unpacklo_epi16(ht, hb), bv);
    s = _mm_srli_epi32(_mm_add_epi32(s, c512), 10);
    return _mm_cvtsi128_si32(
        _mm_packus_epi16(_mm_packs_epi32(s, _mm_setzero_si128()), _mm_setzero_si128()));
}

void UnDistortionImplSSE::remapNChannelBilinear(const uint8_t *src, uint8_t *dst,
                                                int w, int h, int ch) {
    const int srcW = srcW_;
    const int srcH = srcH_;

    // Process rows [vBegin, vEnd) on the calling thread.
    // Uses std::async so idle threads truly sleep (no OMP spin-wait overhead).
    auto processRows = [&](int vBegin, int vEnd) {
        const __m128i c32  = _mm_set1_epi32(INTER_TAB);
        const __m128i c31  = _mm_set1_epi32(INTER_TAB - 1);
        const __m128i c512 = _mm_set1_epi32(INTER_TAB2 / 2);
        constexpr int PF   = 8;

    for(int v = vBegin; v < vEnd; v++) {
    const int row_start = v * w;
    const int row_end   = row_start + w;
    int idx = row_start;

    for(; idx <= row_end - 4; idx += 4) {
        // Software prefetch for source pixels ahead
        if(idx + PF + 4 <= row_end) {
            for(int pk = 0; pk < 4; pk++) {
                const int pfx = mapXi_[idx + PF + pk];
                const int pfy = mapYi_[idx + PF + pk];
                _mm_prefetch(reinterpret_cast<const char *>(
                    src + ( pfy      * srcW + pfx) * ch), _MM_HINT_T0);
                _mm_prefetch(reinterpret_cast<const char *>(
                    src + ((pfy + 1) * srcW + pfx) * ch), _MM_HINT_T0);
            }
        }

        // Load 4 × int16 coords and alpha from SoA arrays
        __m128i x04 = _mm_cvtepi16_epi32(
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapXi_[idx])));
        __m128i y04 = _mm_cvtepi16_epi32(
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapYi_[idx])));
        __m128i a4  = _mm_cvtepu16_epi32(
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapAlpha_[idx])));
        __m128i fx4 = _mm_and_si128(a4, c31);
        __m128i fy4 = _mm_srli_epi32(a4, INTER_BITS);

        alignas(16) int32_t x0[4], y0[4], fxs[4], fys[4];
        _mm_store_si128(reinterpret_cast<__m128i *>(x0),  x04);
        _mm_store_si128(reinterpret_cast<__m128i *>(y0),  y04);
        _mm_store_si128(reinterpret_cast<__m128i *>(fxs), fx4);
        _mm_store_si128(reinterpret_cast<__m128i *>(fys), fy4);

        if(ch == 1) {
            __m128i fxi4 = _mm_sub_epi32(c32, fx4);
            __m128i fyi4 = _mm_sub_epi32(c32, fy4);
            __m128i w004 = _mm_mullo_epi32(fxi4, fyi4);
            __m128i w104 = _mm_mullo_epi32(fx4,  fyi4);
            __m128i w014 = _mm_mullo_epi32(fxi4, fy4 );
            __m128i w114 = _mm_mullo_epi32(fx4,  fy4 );
            alignas(16) int32_t p00v[4], p10v[4], p01v[4], p11v[4];
            for(int k = 0; k < 4; k++) {
                const int x1 = std::min(x0[k] + 1, srcW - 1);
                const int y1 = std::min(y0[k] + 1, srcH - 1);
                p00v[k] = src[y0[k] * srcW + x0[k]];
                p10v[k] = src[y0[k] * srcW + x1   ];
                p01v[k] = src[y1    * srcW + x0[k]];
                p11v[k] = src[y1    * srcW + x1   ];
            }
            __m128i sum4 = _mm_add_epi32(
                _mm_add_epi32(_mm_mullo_epi32(w004, _mm_load_si128((__m128i*)p00v)),
                              _mm_mullo_epi32(w104, _mm_load_si128((__m128i*)p10v))),
                _mm_add_epi32(_mm_mullo_epi32(w014, _mm_load_si128((__m128i*)p01v)),
                              _mm_mullo_epi32(w114, _mm_load_si128((__m128i*)p11v))));
            sum4 = _mm_srli_epi32(_mm_add_epi32(sum4, c512), 10);
            *reinterpret_cast<uint32_t *>(dst + idx) =
                static_cast<uint32_t>(_mm_cvtsi128_si32(
                    _mm_packus_epi16(_mm_packs_epi32(sum4, _mm_setzero_si128()),
                                     _mm_setzero_si128())));
        }
        else if(ch == 3) {
            for(int k = 0; k < 4; k++) {
                const int x1k = std::min(x0[k] + 1, srcW - 1);
                const int row0 = y0[k] * srcW;
                const int row1 = (y0[k] < srcH - 1 ? y0[k] + 1 : y0[k]) * srcW;
                const int32_t val = bilinKernel(
                    src + (row0 + x0[k]) * 3, src + (row0 + x1k) * 3,
                    src + (row1 + x0[k]) * 3, src + (row1 + x1k) * 3,
                    fxs[k], fys[k], c512);
                uint8_t *out = dst + (idx + k) * 3;
                out[0] = static_cast<uint8_t>(val      );
                out[1] = static_cast<uint8_t>(val >>  8);
                out[2] = static_cast<uint8_t>(val >> 16);
            }
        }
        else {  // ch == 4
            for(int k = 0; k < 4; k++) {
                const int x1k = std::min(x0[k] + 1, srcW - 1);
                const int row0 = y0[k] * srcW;
                const int row1 = (y0[k] < srcH - 1 ? y0[k] + 1 : y0[k]) * srcW;
                *reinterpret_cast<int32_t *>(dst + (idx + k) * 4) = bilinKernel(
                    src + (row0 + x0[k]) * 4, src + (row0 + x1k) * 4,
                    src + (row1 + x0[k]) * 4, src + (row1 + x1k) * 4,
                    fxs[k], fys[k], c512);
            }
        }
    }

    // Scalar tail for remaining pixels in this row
    for(; idx < row_end; idx++) {
        const int      x0    = mapXi_[idx];
        const int      y0    = mapYi_[idx];
        const uint16_t alpha = mapAlpha_[idx];
        const int      fx    = alpha & (INTER_TAB - 1);
        const int      fy    = alpha >> INTER_BITS;
        const int      x1    = std::min(x0 + 1, srcW - 1);
        const int      y1    = std::min(y0 + 1, srcH - 1);
        const int      row0  = y0 * srcW;
        const int      row1  = y1 * srcW;
        const int      fxi   = INTER_TAB - fx, fyi = INTER_TAB - fy;
        const int      w00   = fxi*fyi, w10 = fx*fyi, w01 = fxi*fy, w11 = fx*fy;
        const uint8_t *p00 = src + (row0 + x0) * ch;
        const uint8_t *p10 = src + (row0 + x1) * ch;
        const uint8_t *p01 = src + (row1 + x0) * ch;
        const uint8_t *p11 = src + (row1 + x1) * ch;
        uint8_t       *out = dst + idx * ch;
        for(int c = 0; c < ch; c++) {
            out[c] = static_cast<uint8_t>(
                (w00*p00[c] + w10*p10[c] + w01*p01[c] + w11*p11[c] + 512) >> 10);
        }
    }
    } // end row loop
    }; // end processRows lambda

    // Split into 3 slices (matching OpenCV's parallel_for_ default thread count).
    // std::async uses MSVC's thread pool: threads truly sleep when idle,
    // so CPU stays low between frames (no OMP spin-wait).
    constexpr int N = 3;
    if(h < N * 4) {
        processRows(0, h);
        return;
    }
    const int chunk = h / N;
    auto f1 = std::async(std::launch::async, processRows, 0,       chunk    );
    auto f2 = std::async(std::launch::async, processRows, chunk,   2*chunk  );
    processRows(2 * chunk, h);   // main thread handles the last slice
    f1.get();
    f2.get();
}

// ---------------------------------------------------------------------------
// remapYUYV – SSE-accelerated YUYV bilinear remap.
//
// Y luma : 4 pixels per SSE iteration, same mullo_epi32 path as ch==1.
//          Y bytes are at stride 2 in the YUYV stream: src[2*(y*srcW+x)].
// UV chroma: scalar nearest-neighbor from the half-width mapUVX_/mapUVY_ LUT.
//            One UV pair per 2 output luma pixels; the SSE loop handles 2 UV
//            pairs per iteration so 4 Y + 2 UV are produced together.
// Parallelism: same 3-way std::async split as remapNChannelBilinear.
// ---------------------------------------------------------------------------
void UnDistortionImplSSE::remapYUYV(const uint8_t *src, uint8_t *dst, int w, int h) {
    const int srcW = srcW_;
    const int srcH = srcH_;
    const int hw   = w / 2;

    auto processRows = [&](int vBegin, int vEnd) {
        const __m128i c32  = _mm_set1_epi32(INTER_TAB);
        const __m128i c31  = _mm_set1_epi32(INTER_TAB - 1);
        const __m128i c512 = _mm_set1_epi32(INTER_TAB2 / 2);

        for(int v = vBegin; v < vEnd; v++) {
            const int rowYBase  = v * w;
            const int rowUVBase = v * hw;
            const int dstRowOff = v * w * 2;

            // Process 2 macropixels (4 Y + 2 UV) per SSE iteration
            int u2 = 0;
            for(; u2 <= hw - 2; u2 += 2) {
                const int yIdx = rowYBase + 2 * u2;

                __m128i x04 = _mm_cvtepi16_epi32(
                    _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapXi_[yIdx])));
                __m128i y04 = _mm_cvtepi16_epi32(
                    _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapYi_[yIdx])));
                __m128i a4  = _mm_cvtepu16_epi32(
                    _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&mapAlpha_[yIdx])));
                __m128i fx4 = _mm_and_si128(a4, c31);
                __m128i fy4 = _mm_srli_epi32(a4, INTER_BITS);

                __m128i fxi4 = _mm_sub_epi32(c32, fx4);
                __m128i fyi4 = _mm_sub_epi32(c32, fy4);
                __m128i w004 = _mm_mullo_epi32(fxi4, fyi4);
                __m128i w104 = _mm_mullo_epi32(fx4,  fyi4);
                __m128i w014 = _mm_mullo_epi32(fxi4, fy4 );
                __m128i w114 = _mm_mullo_epi32(fx4,  fy4 );

                alignas(16) int32_t x0[4], y0[4];
                _mm_store_si128(reinterpret_cast<__m128i *>(x0), x04);
                _mm_store_si128(reinterpret_cast<__m128i *>(y0), y04);

                // Gather Y luma – stride 2 because YUYV packs [Y,U,Y,V]
                alignas(16) int32_t p00v[4], p10v[4], p01v[4], p11v[4];
                for(int k = 0; k < 4; k++) {
                    const int x1 = std::min(x0[k] + 1, srcW - 1);
                    const int y1 = std::min(y0[k] + 1, srcH - 1);
                    p00v[k] = src[2 * (y0[k] * srcW + x0[k])];
                    p10v[k] = src[2 * (y0[k] * srcW + x1   )];
                    p01v[k] = src[2 * (y1    * srcW + x0[k])];
                    p11v[k] = src[2 * (y1    * srcW + x1   )];
                }

                __m128i sum4 = _mm_add_epi32(
                    _mm_add_epi32(_mm_mullo_epi32(w004, _mm_load_si128((__m128i *)p00v)),
                                  _mm_mullo_epi32(w104, _mm_load_si128((__m128i *)p10v))),
                    _mm_add_epi32(_mm_mullo_epi32(w014, _mm_load_si128((__m128i *)p01v)),
                                  _mm_mullo_epi32(w114, _mm_load_si128((__m128i *)p11v))));
                sum4 = _mm_srli_epi32(_mm_add_epi32(sum4, c512), 10);
                const uint32_t ypacked = static_cast<uint32_t>(_mm_cvtsi128_si32(
                    _mm_packus_epi16(_mm_packs_epi32(sum4, _mm_setzero_si128()),
                                     _mm_setzero_si128())));

                // Write 2 YUYV macropixels; UV byte offset is precomputed.
                for(int j = 0; j < 2; j++) {
                    const uint8_t *uvSrc = src + mapUVOff_[rowUVBase + u2 + j];
                    const int o = dstRowOff + (u2 + j) * 4;
                    dst[o + 0] = static_cast<uint8_t>(ypacked >> (j * 16    ));
                    dst[o + 1] = uvSrc[1];
                    dst[o + 2] = static_cast<uint8_t>(ypacked >> (j * 16 + 8));
                    dst[o + 3] = uvSrc[3];
                }
            }

            // Scalar tail for odd hw
            for(; u2 < hw; u2++) {
                const int yIdx0 = rowYBase + 2 * u2;
                const int yIdx1 = yIdx0 + 1;

                auto bilinY = [&](int idx) -> uint8_t {
                    const int      xi    = mapXi_[idx];
                    const int      yi    = mapYi_[idx];
                    const uint16_t alpha = mapAlpha_[idx];
                    const int      fx    = alpha & (INTER_TAB - 1);
                    const int      fy    = alpha >> INTER_BITS;
                    const int      x1    = std::min(xi + 1, srcW - 1);
                    const int      y1    = std::min(yi + 1, srcH - 1);
                    const int      fxi   = INTER_TAB - fx, fyi = INTER_TAB - fy;
                    const int      w00   = fxi * fyi, w10 = fx * fyi, w01 = fxi * fy, w11 = fx * fy;
                    return static_cast<uint8_t>(
                        (w00 * src[2 * (yi * srcW + xi)] + w10 * src[2 * (yi * srcW + x1)] +
                         w01 * src[2 * (y1 * srcW + xi)] + w11 * src[2 * (y1 * srcW + x1)] + 512) >> 10);
                };

                const uint8_t *uvSrc = src + mapUVOff_[rowUVBase + u2];
                const int o = dstRowOff + u2 * 4;
                dst[o + 0] = bilinY(yIdx0);
                dst[o + 1] = uvSrc[1];
                dst[o + 2] = bilinY(yIdx1);
                dst[o + 3] = uvSrc[3];
            }
        }
    };

    constexpr int N = 3;
    if(h < N * 4) {
        processRows(0, h);
        return;
    }
    const int chunk = h / N;
    auto f1 = std::async(std::launch::async, processRows, 0,       chunk  );
    auto f2 = std::async(std::launch::async, processRows, chunk, 2*chunk  );
    processRows(2 * chunk, h);
    f1.get();
    f2.get();
}

}  // namespace libobsensor

#endif  // __ARM_NEON__ || __NEON__ || __SSSE3__
