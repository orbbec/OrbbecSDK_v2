// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#ifndef D2C_DEPTH_TO_COLOR_IMPL_H
#define D2C_DEPTH_TO_COLOR_IMPL_H

#include <string>
#include <utility>
#include <unordered_map>
#include <memory>
#include "libobsensor/h/ObTypes.h"

#if(defined(__ARM_NEON__) || defined(__aarch64__) || defined(__arm__))
#include "SSE2NEON.h"
#else
#include <xmmintrin.h>
#include <smmintrin.h>
#endif

namespace libobsensor {

#define EPSILON (1e-6)

struct ResHashFunc {
    size_t operator()(const std::pair<int, int> &p) const {
        return (((uint32_t)p.first & 0xFFFF) << 16) | ((uint32_t)p.second & 0xFFFF);
    }
};

struct ResComp {
    bool operator()(const std::pair<int, int> &p1, const std::pair<int, int> &p2) const {
        return p1.first == p2.first && p1.second == p2.second;
    }
};

/**
 * @brief Implementation of Alignment
 */
class AlignImpl {
public:
    AlignImpl();

    ~AlignImpl();

    /**
     * @brief Load parameters and set up LUT
     * @param[in] depth_intrin intrisics of depth frame
     * @param[in] depth_disto distortion parameters of depth frame
     * @param[in] rgb_intrin intrinsics of color frame
     * @param[in] rgb_disto distortion parameters of color frame
     * @param[in] depth_to_rgb rotation and translation from depth to color
     * @param[in] depth_unit_mm depth scale in millimeter
     * @param[in] add_target_distortion switch to add distortion of the target frame
     * @param[in] gap_fill_copy switch to fill gaps with copy or nearest-interpolation after alignment
     *
     */
    void initialize(OBCameraIntrinsic depth_intrin, OBCameraDistortion depth_disto, OBCameraIntrinsic rgb_intrin, OBCameraDistortion rgb_disto,
                    OBExtrinsic depth_to_rgb, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy);

    /**
     * @brief Get depth unit in millimeter
     *
     * @return float depth scale in millimeter
     */
    float getDepthUnit() {
        return depth_unit_mm_;
    };

    /**
     * @brief Prepare LUTs of depth undistortion and rotation
     */
    void prepareDepthResolution();

    /**
     * @brief Clear buffer and de-initialize
     */
    void reset();

    /**
     * @brief       Align depth to color
     * @param[in]       depth_buffer  data buffer of the depth frame in row-major order
     * @param[in]       depth_width   width of the depth frame
     * @param[in]       depth_height  height of the depth frame
     * @param[out]  out_depth     aligned data buffer in row-major order
     * @param[in]       color_width   width of the color frame
     * @param[in]       color_height  height of the color frame
     * @param[in]       map    coordinate mapping for C2D
     * @param[in]       withSSE switch to speed up with SSE
     * @retval  -1  fail
     * @retval  0   succeed
     */
    int D2C(const uint16_t *depth_buffer, int depth_width, int depth_height, uint16_t *out_depth, int color_width, int color_height, int *map = nullptr,
            bool withSSE = true);

    /**
     * @brief Align color to depth
     * @param[in] depth_buffer data buffer of the depth frame in row-major order
     * @param[in] depth_width  width of the depth frame
     * @param[in] depth_height height of the depth frame
     * @param[in] rgb_buffer   data buffer the to-align color frame
     * @param[out] out_rgb      aligned data buffer of the color frame
     * @param[in] color_width  width of the to-align color frame
     * @param[in] color_height height of the to-align color frame
     * @param[in] format       pixel format of the color fraem
     * @retval -1 fail
     * @retval 0 succeed
     */
    int C2D(const uint16_t *depth_buffer, int depth_width, int depth_height, const void *rgb_buffer, void *out_rgb, int color_width, int color_height,
            OBFormat format, bool withSSE = true);

private:
    void clearMatrixCache();

    void D2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_x, const float *coeff_y, const float *coeff_z, int *map = nullptr);
    void D2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_x, const float *coeff_y, const float *coeff_z, int *map = nullptr);
    /** SSE speed-ed depth to color alignment with different distortion model */
    void distortedWithSSE(__m128 &nx, __m128 &ny, const __m128 x2, const __m128 y2, const __m128 r2);
    void KBDistortedWithSSE(__m128 &nx, __m128 &ny, const __m128 r2);
    void BMDistortedWithSSE(__m128 &nx, __m128 &ny, const __m128 x2, const __m128 y2, const __m128 r2);

    void transferDepth(const float *pixelx, const float *pixely, const float *depth, const int npts, const int point_index, uint16_t *out_depth, int *map);

    /**
     * @brief               Transfer pixels of the source image buffer to the target
     * @tparam T            pixel type, like uint8_t, uint16_t, etc.
     * @param map           coordinate map of the same dimensions as the target
     * @param src_buffer    the source image buffer
     * @param src_width     columns of the source image
     * @param src_height    rows of the source image
     * @param dst_buffer    the target image buffer
     * @param dst_width
     */
    template <typename T> void mapPixel(const int *map, const T *src_buffer, int src_width, int src_height, T *dst_buffer, int dst_width, int dst_height);

private:
    bool initialized_;

    /** Linear/Distortion Rotation Coeff hash table */
    std::unordered_map<std::pair<int, int>, float *, ResHashFunc, ResComp> rot_coeff_ht_x;
    std::unordered_map<std::pair<int, int>, float *, ResHashFunc, ResComp> rot_coeff_ht_y;
    std::unordered_map<std::pair<int, int>, float *, ResHashFunc, ResComp> rot_coeff_ht_z;

    float              depth_unit_mm_;          // depth scale
    bool               add_target_distortion_;  // distort align frame with target coefficent
    bool               need_to_undistort_depth_;    // undistort depth is necessary
    bool               gap_fill_copy_;          // filling cracks with copy
    OBCameraIntrinsic  depth_intric_{};
    OBCameraIntrinsic  rgb_intric_{};
    OBCameraDistortion depth_disto_{};
    OBCameraDistortion rgb_disto_{};
    OBExtrinsic        transform_{};              // should be depth-to-rgb all the time
    float              scaled_trans_[3] = { 0 };  // scaled translation

    // possible inflection point of the calibrated K6 distortion curve
    float r2_max_loc_;

    // members for SSE
    __m128 color_cx_;
    __m128 color_cy_;
    __m128 color_fx_;
    __m128 color_fy_;
    __m128 color_k1_;
    __m128 color_k2_;
    __m128 color_k3_;
    __m128 color_k4_;
    __m128 color_k5_;
    __m128 color_k6_;
    __m128 color_p1_;
    __m128 color_p2_;
    __m128 scaled_trans_1_;
    __m128 scaled_trans_2_;
    __m128 scaled_trans_3_;
    __m128 r2_max_loc_sse_;

    const static __m128  POINT_FIVE;
    const static __m128  TWO;
    const static __m128i ZERO;
    const static __m128  ZERO_F;
};

#endif  // D2C_DEPTH_TO_COLOR_IMPL_H
}  // namespace

