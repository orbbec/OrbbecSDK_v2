// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if !(defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__))

#include "AlignImplGeneric.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <vector>

namespace libobsensor {
namespace {
// Keep the scalar projection independent of SIMD and image bounds: a pixel's
// footprint may cross an image edge and is clipped only when writing output.
bool distort(const OBCameraDistortion &d, float x, float y, OBPoint2f &out) {
    const float r2     = x * x + y * y;
    float       radial = 1.0f;
    float       dx = 0.0f, dy = 0.0f;
    if(d.model == OB_DISTORTION_BROWN_CONRADY || d.model == OB_DISTORTION_BROWN_CONRADY_K6) {
        const float r4 = r2 * r2, r6 = r4 * r2;
        radial = 1 + d.k1 * r2 + d.k2 * r4 + d.k3 * r6;
        if(d.model == OB_DISTORTION_BROWN_CONRADY_K6) {
            const float denominator = 1 + d.k4 * r2 + d.k5 * r4 + d.k6 * r6;
            if(std::fabs(denominator) < 1e-8f) {
                return false;
            }
            radial /= denominator;
        }
        dx = 2 * d.p1 * x * y + d.p2 * (r2 + 2 * x * x);
        dy = d.p1 * (r2 + 2 * y * y) + 2 * d.p2 * x * y;
    }
    else if(d.model == OB_DISTORTION_KANNALA_BRANDT4) {
        const float r = std::sqrt(r2);
        if(r > 1e-8f) {
            const float theta = std::atan(r), t2 = theta * theta;
            radial = theta * (1 + t2 * (d.k1 + t2 * (d.k2 + t2 * (d.k3 + t2 * d.k4)))) / r;
        }
    }
    else if(d.model != OB_DISTORTION_NONE) {
        return false;
    }
    out = { x * radial + dx, y * radial + dy };
    return std::isfinite(out.x) && std::isfinite(out.y);
}

bool unproject(const OBCameraIntrinsic &intrinsic, const OBCameraDistortion &d, float u, float v, OBPoint2f &ray) {
    const float xd = (u - intrinsic.cx) / intrinsic.fx, yd = (v - intrinsic.cy) / intrinsic.fy;
    ray = { xd, yd };
    for(int i = 0; i < 20; ++i) {
        OBPoint2f projected{};
        if(!distort(d, ray.x, ray.y, projected)) {
            return false;
        }
        const float ex = xd - projected.x, ey = yd - projected.y;
        if(std::fabs(ex * intrinsic.fx) + std::fabs(ey * intrinsic.fy) < 0.01f) {
            return true;
        }
        ray.x += ex;
        ray.y += ey;
    }
    return false;
}
}  // namespace

AlignImplGeneric::AlignImplGeneric() {}

AlignImplGeneric::~AlignImplGeneric() {}

void AlignImplGeneric::initialize(OBCameraIntrinsic depth_intrin, OBCameraDistortion depth_disto, OBCameraIntrinsic rgb_intrin, OBCameraDistortion rgb_disto,
                                  OBExtrinsic depth_to_rgb, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy, bool use_scale,
                                  OBFormat depth_format, uint16_t max_invalid_value) {
    reset();
    if(depth_intrin.width <= 0 || depth_intrin.height <= 0 || rgb_intrin.width <= 0 || rgb_intrin.height <= 0 || !std::isfinite(depth_unit_mm)
       || depth_unit_mm <= 0 || !std::isfinite(depth_intrin.fx) || depth_intrin.fx <= 0 || !std::isfinite(depth_intrin.fy) || depth_intrin.fy <= 0
       || !std::isfinite(rgb_intrin.fx) || rgb_intrin.fx <= 0 || !std::isfinite(rgb_intrin.fy) || rgb_intrin.fy <= 0 || max_invalid_value == 0) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid generic alignment calibration");
    }
    depth_format_          = depth_format;
    depth_intric_          = depth_intrin;
    depth_disto_           = depth_disto;
    rgb_intric_            = rgb_intrin;
    rgb_disto_             = rgb_disto;
    transform_             = depth_to_rgb;
    depth_unit_mm_         = depth_unit_mm > 0.0f ? depth_unit_mm : 1.0f;
    add_target_distortion_ = add_target_distortion;
    gap_fill_copy_         = gap_fill_copy;
    max_invalid_value_     = max_invalid_value;
    initialized_           = true;

    // Resolution scaling is an optimization; scalar projection uses the requested resolution.
    (void)use_scale;
}

float AlignImplGeneric::getDepthUnit() const {
    return depth_unit_mm_;
}

void AlignImplGeneric::prepareDepthResolution() {
    // The generic path computes coordinates directly and does not need architecture-specific LUTs.
}

void AlignImplGeneric::reset() {
    initialized_ = false;
}

int AlignImplGeneric::D2C(const uint16_t *depth_buffer, int depth_width, int depth_height, uint16_t *out_depth, int color_width, int color_height, int *map,
                          bool withSSE) {
    (void)withSSE;

    if(!initialized_ || !depth_buffer || (!out_depth && !map) || depth_width != depth_intric_.width || depth_height != depth_intric_.height
       || color_width != rgb_intric_.width || color_height != rgb_intric_.height || depth_width <= 0 || depth_height <= 0 || color_width <= 0
       || color_height <= 0) {
        LOG_ERROR("Generic align is not initialized or input parameters do not match");
        return -1;
    }

    if(out_depth) {
        std::memset(out_depth, 0, static_cast<size_t>(color_width) * color_height * sizeof(uint16_t));
    }
    if(map) {
        std::fill(map, map + static_cast<size_t>(depth_width) * depth_height * 2, -1);
    }

    OBCameraDistortion targetDistortion{};
    if(add_target_distortion_) {
        targetDistortion = rgb_disto_;
    }

    auto project = [&](float x, float y, uint16_t packedDepth, OBPoint2f &pixel, uint16_t &targetDepth) {
        OBPoint2f ray{};
        if(!unproject(depth_intric_, depth_disto_, x, y, ray)) {
            return false;
        }
        const uint16_t depth       = depth_format_ == OB_FORMAT_Y12C4 ? packedDepth >> 4 : packedDepth;
        const float    z           = depth * depth_unit_mm_;
        const float    tx          = z * (transform_.rot[0] * ray.x + transform_.rot[1] * ray.y + transform_.rot[2]) + transform_.trans[0];
        const float    ty          = z * (transform_.rot[3] * ray.x + transform_.rot[4] * ray.y + transform_.rot[5]) + transform_.trans[1];
        const float    tz          = z * (transform_.rot[6] * ray.x + transform_.rot[7] * ray.y + transform_.rot[8]) + transform_.trans[2];
        const float    targetValue = tz / depth_unit_mm_;
        const uint16_t limit       = depth_format_ == OB_FORMAT_Y12C4 ? std::min<uint16_t>(4096, max_invalid_value_) : max_invalid_value_;
        if(!std::isfinite(targetValue) || targetValue < 0.5f || targetValue >= limit - 0.5f || !distort(targetDistortion, tx / tz, ty / tz, pixel)) {
            return false;
        }
        pixel.x = pixel.x * rgb_intric_.fx + rgb_intric_.cx;
        pixel.y = pixel.y * rgb_intric_.fy + rgb_intric_.cy;
        // Bound coordinates before float-to-int conversion, including extreme calibration values.
        if(!std::isfinite(pixel.x) || !std::isfinite(pixel.y)) {
            return false;
        }
        pixel.x     = std::max(-1.0f, std::min(static_cast<float>(color_width), pixel.x));
        pixel.y     = std::max(-1.0f, std::min(static_cast<float>(color_height), pixel.y));
        targetDepth = static_cast<uint16_t>(targetValue + 0.5f);
        if(depth_format_ == OB_FORMAT_Y12C4) {
            targetDepth = static_cast<uint16_t>((targetDepth << 4) | (packedDepth & 0xf));
        }
        return true;
    };

    auto writeDepth = [&](int x, int y, uint16_t depth) {
        if(!out_depth || x < 0 || x >= color_width || y < 0 || y >= color_height) {
            return;
        }
        auto &value = out_depth[y * color_width + x];
        if(value == 0 || depth < value) {
            value = depth;
        }
    };

    for(int y = 0; y < depth_height; ++y) {
        for(int x = 0; x < depth_width; ++x) {
            int      depthIndex = y * depth_width + x;
            uint16_t depth      = depth_buffer[depthIndex];
            if((depth_format_ == OB_FORMAT_Y12C4 ? depth >> 4 : depth) == 0 || (depth_format_ == OB_FORMAT_Y12C4 ? depth >> 4 : depth) >= max_invalid_value_) {
                continue;
            }

            OBPoint2f centerPixel{};
            uint16_t  centerDepth = 0;
            if(!project(static_cast<float>(x), static_cast<float>(y), depth, centerPixel, centerDepth)) {
                continue;
            }

            int centerX = static_cast<int>(std::floor(centerPixel.x + 0.5f));
            int centerY = static_cast<int>(std::floor(centerPixel.y + 0.5f));
            if(centerX >= 0 && centerX < color_width && centerY >= 0 && centerY < color_height && map) {
                map[2 * depthIndex]     = centerX;
                map[2 * depthIndex + 1] = centerY;
            }

            if(gap_fill_copy_) {
                if(centerX < 0 || centerX >= color_width || centerY < 0 || centerY >= color_height) {
                    continue;
                }
                writeDepth(centerX, centerY, centerDepth);
                writeDepth(centerX + 1, centerY, centerDepth);
                writeDepth(centerX, centerY + 1, centerDepth);
                writeDepth(centerX + 1, centerY + 1, centerDepth);
                continue;
            }

            OBPoint2f cornerPixels[2]{};
            uint16_t  cornerDepths[2]{};
            bool      haveFirst = project(static_cast<float>(x) - 0.5f, static_cast<float>(y) - 0.5f, depth, cornerPixels[0], cornerDepths[0]);
            bool      haveLast  = project(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, depth, cornerPixels[1], cornerDepths[1]);
            if(!haveFirst || !haveLast) {
                writeDepth(centerX, centerY, centerDepth);
                continue;
            }

            int      x0        = std::max(0, static_cast<int>(std::floor(std::min(cornerPixels[0].x, cornerPixels[1].x) + 0.5f)));
            int      y0        = std::max(0, static_cast<int>(std::floor(std::min(cornerPixels[0].y, cornerPixels[1].y) + 0.5f)));
            int      x1        = std::min(color_width - 1, static_cast<int>(std::floor(std::max(cornerPixels[0].x, cornerPixels[1].x) + 0.5f)));
            int      y1        = std::min(color_height - 1, static_cast<int>(std::floor(std::max(cornerPixels[0].y, cornerPixels[1].y) + 0.5f)));
            uint16_t fillDepth = std::min(cornerDepths[0], cornerDepths[1]);
            for(int targetY = y0; targetY <= y1; ++targetY) {
                for(int targetX = x0; targetX <= x1; ++targetX) {
                    writeDepth(targetX, targetY, fillDepth);
                }
            }
        }
    }

    return 0;
}

namespace {
struct Pixel24 {
    uint8_t values[3];
};

template <typename T>
void mapPixels(const std::vector<int> &map, const T *source, int sourceWidth, int sourceHeight, T *target, int targetWidth, int targetHeight) {
    for(int y = 0; y < targetHeight; ++y) {
        for(int x = 0; x < targetWidth; ++x) {
            int targetIndex = y * targetWidth + x;
            int sourceX     = map[2 * targetIndex];
            int sourceY     = map[2 * targetIndex + 1];
            if(sourceX >= 0 && sourceX < sourceWidth && sourceY >= 0 && sourceY < sourceHeight) {
                target[targetIndex] = source[sourceY * sourceWidth + sourceX];
            }
        }
    }
}
}  // namespace

int AlignImplGeneric::C2D(const uint16_t *depth_buffer, int depth_width, int depth_height, const void *rgb_buffer, void *out_rgb, int color_width,
                          int color_height, OBFormat format, bool withSSE) {
    if(!initialized_ || !rgb_buffer || !out_rgb || depth_width <= 0 || depth_height <= 0 || depth_width != depth_intric_.width
       || depth_height != depth_intric_.height || color_width != rgb_intric_.width || color_height != rgb_intric_.height) {
        LOG_ERROR("Generic C2D buffers are not initialized");
        return -1;
    }

    std::vector<int> depthToColor(static_cast<size_t>(depth_width) * depth_height * 2, -1);
    if(D2C(depth_buffer, depth_width, depth_height, nullptr, color_width, color_height, depthToColor.data(), withSSE) != 0) {
        return -1;
    }

    size_t targetPixels = static_cast<size_t>(depth_width) * depth_height;
    switch(format) {
    case OB_FORMAT_Y8:
        std::memset(out_rgb, 0, targetPixels * sizeof(uint8_t));
        mapPixels(depthToColor, static_cast<const uint8_t *>(rgb_buffer), color_width, color_height, static_cast<uint8_t *>(out_rgb), depth_width,
                  depth_height);
        break;
    case OB_FORMAT_Y16:
        std::memset(out_rgb, 0, targetPixels * sizeof(uint16_t));
        mapPixels(depthToColor, static_cast<const uint16_t *>(rgb_buffer), color_width, color_height, static_cast<uint16_t *>(out_rgb), depth_width,
                  depth_height);
        break;
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
        std::memset(out_rgb, 0, targetPixels * sizeof(Pixel24));
        mapPixels(depthToColor, static_cast<const Pixel24 *>(rgb_buffer), color_width, color_height, static_cast<Pixel24 *>(out_rgb), depth_width,
                  depth_height);
        break;
    case OB_FORMAT_BGRA:
    case OB_FORMAT_RGBA:
        std::memset(out_rgb, 0, targetPixels * sizeof(uint32_t));
        mapPixels(depthToColor, static_cast<const uint32_t *>(rgb_buffer), color_width, color_height, static_cast<uint32_t *>(out_rgb), depth_width,
                  depth_height);
        break;
    default:
        LOG_ERROR("Unsupported format for generic C2D conversion");
        return -1;
    }

    return 0;
}

}  // namespace libobsensor

#endif  // __ARM_NEON__ || __NEON__ || __SSSE3__
