// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

class IAlignImpl {
public:
    virtual ~IAlignImpl() = default;

    /**
     * @brief Load parameters and set up LUT
     *
     * @param[in] depth_intrin intrisics of depth frame
     * @param[in] depth_disto distortion parameters of depth frame
     * @param[in] rgb_intrin intrinsics of color frame
     * @param[in] rgb_disto distortion parameters of color frame
     * @param[in] depth_to_rgb rotation and translation from depth to color
     * @param[in] depth_unit_mm depth scale in millimeter
     * @param[in] add_target_distortion switch to add distortion of the target frame
     * @param[in] gap_fill_copy switch to fill gaps with copy or nearest-interpolation after alignment
     * @param[in] auto_scale_down switch to automatically scale down resolution of depth frame
     */
    virtual void initialize(OBCameraIntrinsic depth_intrin, OBCameraDistortion depth_disto, OBCameraIntrinsic rgb_intrin, OBCameraDistortion rgb_disto,
                            OBExtrinsic depth_to_rgb, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy, bool use_scale = false,
                            OBFormat depth_format = OB_FORMAT_UNKNOWN) = 0;

    /**
     * @brief Get depth unit in millimeter
     *
     * @return float depth scale in millimeter
     */
    virtual float getDepthUnit() const = 0;

    /**
     * @brief Prepare LUTs of depth undistortion and rotation
     */
    virtual void prepareDepthResolution() = 0;

    /**
     * @brief Clear buffer and de-initialize
     */
    virtual void reset() = 0;

    /**
     * @brief Align depth to color
     * @param[in] depth_buffer data buffer of the depth frame in row-major order
     * @param[in] depth_width width of the depth frame
     * @param[in] depth_height height of the depth frame
     * @param[out] out_depth aligned data buffer in row-major order
     * @param[in] color_width width of the color frame
     * @param[in] color_height height of the color frame
     * @param[in] map coordinate mapping for C2D
     * @param[in] withSSE switch to speed up with SSE
     *
     * @retval -1 fail
     * @retval  0 succeed
     */
    virtual int D2C(const uint16_t *depth_buffer, int depth_width, int depth_height, uint16_t *out_depth, int color_width, int color_height, int *map = nullptr,
                    bool withSSE = true) = 0;

    /**
     * @brief Align color to depth
     *
     * @param[in] depth_buffer data buffer of the depth frame in row-major order
     * @param[in] depth_width width of the depth frame
     * @param[in] depth_height height of the depth frame
     * @param[in] rgb_buffer data buffer the to-align color frame
     * @param[out] out_rgb aligned data buffer of the color frame
     * @param[in] color_width  width of the to-align color frame
     * @param[in] color_height height of the to-align color frame
     * @param[in] format pixel format of the color fraem
     *
     * @retval -1 fail
     * @retval 0 succeed
     */
    virtual int C2D(const uint16_t *depth_buffer, int depth_width, int depth_height, const void *rgb_buffer, void *out_rgb, int color_width, int color_height,
                    OBFormat format, bool withSSE = true) = 0;
};

}  // namespace libobsensor
