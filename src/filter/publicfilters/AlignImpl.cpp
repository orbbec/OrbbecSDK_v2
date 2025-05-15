// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AlignImpl.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <complex>

namespace libobsensor {

static inline void addDistortion(const OBCameraDistortion &distort_param, const float pt_ud[2], float pt_d[2]) {
    float k1 = distort_param.k1, k2 = distort_param.k2, k3 = distort_param.k3;
    float k4 = distort_param.k4, k5 = distort_param.k5, k6 = distort_param.k6;
    float p1 = distort_param.p1, p2 = distort_param.p2;

    const float r2 = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];

    if((distort_param.model == OB_DISTORTION_BROWN_CONRADY) || (distort_param.model == OB_DISTORTION_BROWN_CONRADY_K6)) {
        const float r4 = r2 * r2;
        const float r6 = r4 * r2;

        float k_diff = 1 + k1 * r2 + k2 * r4 + k3 * r6;
        if(distort_param.model == OB_DISTORTION_BROWN_CONRADY_K6) {
            k_diff /= (1 + k4 * r2 + k5 * r4 + k6 * r6);
        }

        const float t_x = p2 * (r2 + 2 * pt_ud[0] * pt_ud[0]) + 2 * p1 * pt_ud[0] * pt_ud[1];
        const float t_y = p1 * (r2 + 2 * pt_ud[1] * pt_ud[1]) + 2 * p2 * pt_ud[0] * pt_ud[1];

        pt_d[0] = pt_ud[0] * k_diff + t_x;
        pt_d[1] = pt_ud[1] * k_diff + t_y;
    }
    else if(distort_param.model == OB_DISTORTION_KANNALA_BRANDT4) {
        const double r      = sqrt(r2);
        const double theta  = atan(r);
        const double theta2 = theta * theta;
        const double theta4 = theta2 * theta2;
        const double theta6 = theta2 * theta4;
        const double theta8 = theta4 * theta4;

        const double k_diff = theta * (1 + (k1 * theta2) + (k2 * theta4) + (k3 * theta6) + (k4 * theta8));

        pt_d[0] = static_cast<float>(k_diff / r * pt_ud[0]);
        pt_d[1] = static_cast<float>(k_diff / r * pt_ud[1]);
    }
}

static inline void removeDistortion(const OBCameraDistortion &distort_param, const float pt_d[2], float pt_ud[2]) {
    const float epsilon       = 1e-6f;
    const int   max_iteration = 20;
    float       tmp_p_ud[2]   = { pt_d[0], pt_d[1] };
    memset(pt_ud, 0, sizeof(float) * 2);
    addDistortion(distort_param, tmp_p_ud, pt_ud);

    pt_ud[0]  = pt_ud[0] - tmp_p_ud[0];
    pt_ud[1]  = pt_ud[1] - tmp_p_ud[1];
    int   it  = 0;
    float err = fabs(tmp_p_ud[0] + pt_ud[0] - pt_d[0]) + fabs(tmp_p_ud[1] + pt_ud[1] - pt_d[1]);
    while(err > epsilon && it++ < max_iteration) {
        tmp_p_ud[0] = pt_d[0] - pt_ud[0];
        tmp_p_ud[1] = pt_d[1] - pt_ud[1];
        addDistortion(distort_param, tmp_p_ud, pt_ud);

        pt_ud[0] = pt_ud[0] - tmp_p_ud[0];
        pt_ud[1] = pt_ud[1] - tmp_p_ud[1];
        err      = fabs(tmp_p_ud[0] + pt_ud[0] - pt_d[0]) + fabs(tmp_p_ud[1] + pt_ud[1] - pt_d[1]);
    }

    pt_ud[0] = tmp_p_ud[0];
    pt_ud[1] = tmp_p_ud[1];
}

const __m128  AlignImpl::POINT_FIVE = _mm_set_ps1(0.5);
const __m128  AlignImpl::TWO        = _mm_set_ps1(2);
const __m128i AlignImpl::ZERO       = _mm_setzero_si128();
const __m128  AlignImpl::ZERO_F     = _mm_set_ps1(0.0);

AlignImpl::AlignImpl() : initialized_(false) {
    depth_unit_mm_   = 1.0;
    r2_max_loc_      = 0.0;
    auto_down_scale_ = 1.0;
    memset(&depth_intric_, 0, sizeof(OBCameraIntrinsic));
    memset(&depth_disto_, 0, sizeof(OBCameraDistortion));
    memset(&rgb_intric_, 0, sizeof(OBCameraIntrinsic));
    memset(&rgb_disto_, 0, sizeof(OBCameraDistortion));
}

AlignImpl::~AlignImpl() {
    clearMatrixCache();
    initialized_ = false;
}

void AlignImpl::initialize(OBCameraIntrinsic depth_intrin, OBCameraDistortion depth_disto, OBCameraIntrinsic rgb_intrin, OBCameraDistortion rgb_disto,
                           OBExtrinsic extrin, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy, bool use_scale) {
    if(initialized_ && memcmp(&depth_intrin, &depth_intric_, sizeof(OBCameraIntrinsic)) == 0
       && memcmp(&depth_disto, &depth_disto_, sizeof(OBCameraDistortion)) == 0 && memcmp(&rgb_intrin, &rgb_intric_, sizeof(OBCameraIntrinsic)) == 0
       && memcmp(&rgb_disto, &rgb_disto_, sizeof(OBCameraDistortion)) == 0 && memcmp(&extrin, &transform_, sizeof(OBExtrinsic)) == 0
       && depth_unit_mm == depth_unit_mm_ && add_target_distortion == add_target_distortion_ && gap_fill_copy == gap_fill_copy_ && use_scale == use_scale_) {
        return;
    }
    memcpy(&depth_intric_, &depth_intrin, sizeof(OBCameraIntrinsic));
    memcpy(&depth_disto_, &depth_disto, sizeof(OBCameraDistortion));
    memcpy(&rgb_intric_, &rgb_intrin, sizeof(OBCameraIntrinsic));
    memcpy(&rgb_disto_, &rgb_disto, sizeof(OBCameraDistortion));

    memcpy(&transform_, &extrin, sizeof(OBExtrinsic));
    add_target_distortion_ = add_target_distortion;
    // since undistorted depth (whether d2c or c2d) is necessory ...
    need_to_undistort_depth_ = ((depth_disto.k1 != 0) || (depth_disto.k2 != 0) || (depth_disto.k3 != 0) || (depth_disto.k4 != 0) || (depth_disto.k5 != 0)
                                || (depth_disto.k6 != 0) || (depth_disto.p1 != 0) || (depth_disto.p2 != 0));
    depth_unit_mm_           = depth_unit_mm;
    gap_fill_copy_           = gap_fill_copy;
    use_scale_               = use_scale;
    // Translation is related to depth unit.
    for(int i = 0; i < 3; ++i) {
        scaled_trans_[i] = transform_.trans[i] / depth_unit_mm_;
    }

    color_fx_       = _mm_set_ps1(rgb_intric_.fx);
    color_cx_       = _mm_set_ps1(rgb_intric_.cx);
    color_fy_       = _mm_set_ps1(rgb_intric_.fy);
    color_cy_       = _mm_set_ps1(rgb_intric_.cy);
    color_k1_       = _mm_set_ps1(rgb_disto_.k1);
    color_k2_       = _mm_set_ps1(rgb_disto_.k2);
    color_k3_       = _mm_set_ps1(rgb_disto_.k3);
    color_k4_       = _mm_set_ps1(rgb_disto_.k4);
    color_k5_       = _mm_set_ps1(rgb_disto_.k5);
    color_k6_       = _mm_set_ps1(rgb_disto_.k6);
    color_p1_       = _mm_set_ps1(rgb_disto_.p1);
    color_p2_       = _mm_set_ps1(rgb_disto_.p2);
    scaled_trans_1_ = _mm_set_ps1(scaled_trans_[0]);
    scaled_trans_2_ = _mm_set_ps1(scaled_trans_[1]);
    scaled_trans_3_ = _mm_set_ps1(scaled_trans_[2]);

    prepareDepthResolution();
    setLimitROI();
    initialized_ = true;
    return;
}

void AlignImpl::reset() {
    clearMatrixCache();
    initialized_ = false;
}

float polynomial(float x, float a, float b, float c, float d) {
    return (a * x * x * x + b * x * x + c * x + d);
}

float binarySearch(float left, float right, float a, float b, float c, float d, float tolerance = 1e-4) {
    while((right - left) > tolerance) {
        float mid   = (left + right) / 2.f;
        float f_mid = polynomial(mid, a, b, c, d);
        if(fabs(f_mid) < tolerance)
            return mid;
        else if(f_mid * polynomial(left, a, b, c, d) < 0)
            right = mid;
        else
            left = mid;
    }
    return (left + right) / 2.f;
}

float estimateInflectionPoint(ob_camera_intrinsic depth_intr, ob_camera_intrinsic rgb_intr, ob_camera_distortion disto) {
    float result = 0.f;
    if(OB_DISTORTION_BROWN_CONRADY_K6 == disto.model) {
        // with k6 distortion model, the denominator (involving k4~k6) should should not be zero
        // the following solves the polynominal function with binary search
        // then a inflection point should be labeled there
        float r2_min = 0.f, r2_max = 0.f;
        {
            float               r2[2];
            ob_camera_intrinsic intrs[] = { depth_intr, rgb_intr };
            for(int i = 0; i < 2; i++) {
                ob_camera_intrinsic intr = intrs[i];
                float               u    = (intr.cx > (intr.width - intr.cx)) ? intr.cx : (intr.width - intr.cx),
                      v                  = (intr.cy > (intr.height - intr.cy)) ? intr.cy : (intr.height - intr.cy);
                float x = u / intr.fx, y = v / intr.fy;
                r2[i] = x * x + y * y;
            }
            if(r2[0] > r2[1]) {
                r2_max = r2[0], r2_min = r2[1];
            }
            else {
                r2_max = r2[1], r2_min = r2[0];
            }
        }

        float c = disto.k4, b = disto.k5, a = disto.k6, d = 1.f;
        float prevX = r2_min;
        float prevF = polynomial(prevX, a, b, c, d);
        for(float r2 = prevX + 0.1f; r2 <= r2_max; r2 += 0.1f) {
            float f = polynomial(r2, a, b, c, d);
            if(prevF * f <= 0) {
                result = binarySearch(prevX, r2, a, b, c, d);
                break;
            }
            prevX = r2;
            prevF = f;
        }
    }
    return result;
}

void AlignImpl::prepareDepthResolution() {
    clearMatrixCache();

    // There may be outliers due to possible inflection points of the calibrated K6 distortion curve;
    if(add_target_distortion_) {
        r2_max_loc_     = estimateInflectionPoint(depth_intric_, rgb_intric_, rgb_disto_);
        r2_max_loc_sse_ = _mm_set_ps1(r2_max_loc_);
    }

    // Prepare LUTs
    int                channel   = (gap_fill_copy_ ? 1 : 2);
    unsigned long long stride    = depth_intric_.width;
    unsigned long long coeff_num = depth_intric_.height * stride;

    float *rot_coeff1[2];
    float *rot_coeff2[2];
    float *rot_coeff3[2];
    for(int i = 0; i < channel; i++) {
        rot_coeff1[i] = new float[coeff_num];
        rot_coeff2[i] = new float[coeff_num];
        rot_coeff3[i] = new float[coeff_num];
    }

    for(int i = 0; i < channel; i++) {
        int mutliplier = (gap_fill_copy_ ? 0 : (i ? 1 : -1));
        for(int v = 0; v < depth_intric_.height; ++v) {
            float *dst1 = rot_coeff1[i] + v * stride;
            float *dst2 = rot_coeff2[i] + v * stride;
            float *dst3 = rot_coeff3[i] + v * stride;
            float  y    = (v + mutliplier * 0.5f - depth_intric_.cy) / depth_intric_.fy;
            for(int u = 0; u < depth_intric_.width; ++u) {
                float x = (u + mutliplier * 0.5f - depth_intric_.cx) / depth_intric_.fx;

                float x_ud = x, y_ud = y;
                if(need_to_undistort_depth_) {
                    float pt_d[2] = { x, y };
                    float pt_ud[2];
                    removeDistortion(depth_disto_, pt_d, pt_ud);
                    x_ud = pt_ud[0];
                    y_ud = pt_ud[1];
                }

                *dst1 = transform_.rot[0] * x_ud + transform_.rot[1] * y_ud + transform_.rot[2];
                *dst2 = transform_.rot[3] * x_ud + transform_.rot[4] * y_ud + transform_.rot[5];
                *dst3 = transform_.rot[6] * x_ud + transform_.rot[7] * y_ud + transform_.rot[8];
                dst1++;
                dst2++;
                dst3++;
            }
        }
    }

    for(int i = 0; i < channel; i++) {
        rot_coeff_ht_x[std::make_pair(depth_intric_.width, depth_intric_.height)][i] = rot_coeff1[i];
        rot_coeff_ht_y[std::make_pair(depth_intric_.width, depth_intric_.height)][i] = rot_coeff2[i];
        rot_coeff_ht_z[std::make_pair(depth_intric_.width, depth_intric_.height)][i] = rot_coeff3[i];
    }
}

void AlignImpl::clearMatrixCache() {
    for(auto item: rot_coeff_ht_x) {
        delete[] item.second[0];
        delete[] item.second[1];
    }
    for(auto item: rot_coeff_ht_y) {
        delete[] item.second[0];
        delete[] item.second[1];
    }
    for(auto item: rot_coeff_ht_z) {
        delete[] item.second[0];
        delete[] item.second[1];
    }

    rot_coeff_ht_x.clear();
    rot_coeff_ht_y.clear();
    rot_coeff_ht_z.clear();
}

void AlignImpl::setLimitROI() {
    x1_limit = _mm_set_ps1(0.0f);
    x2_limit = _mm_set_ps1(static_cast<float>(rgb_intric_.width - 1));
    y1_limit = _mm_set_ps1(0.0f);
    y2_limit = _mm_set_ps1(static_cast<float>(rgb_intric_.height - 1));
}

template <typename T> void fillPixelGap(const int *u, const int *v, const int width, const int height, const T val, T *buffer, bool copy = true) {
    // point index and output depth buffer should be checked outside

    if(copy) {
        if((u[0] >= 0) && (u[0] < width) && (v[0] >= 0) && (v[0] < height)) {
            int pos          = v[0] * width + u[0];
            buffer[pos]      = std::min(buffer[pos], val);
            bool right_valid = (u[0] + 1) < width, bottom_valid = (v[0] + 1) < height;
            if(right_valid) {
                if(buffer[pos + 1] > val)
                    buffer[pos + 1] = val;
            }
            if(bottom_valid) {
                if(buffer[pos + width] > val)
                    buffer[pos + width] = val;
            }
            if(right_valid && bottom_valid) {
                if(buffer[pos + width + 1] > val)
                    buffer[pos + width + 1] = val;
            }
        }
    }
    else {
        int u0 = std::max(0, std::min(u[0], u[1]));
        int v0 = std::max(0, std::min(v[0], v[1]));
        int u1 = std::min(std::max(u[0], u[1]), width - 1);
        int v1 = std::min(std::max(v[0], v[1]), height - 1);
        for(int vr = v0; vr <= v1; vr++) {
            for(int ur = u0; ur <= u1; ur++) {
                int pos = vr * width + ur;
                if(buffer[pos] > val)
                    buffer[pos] = val;
            }
        }
    }
}

void AlignImpl::distortedWithSSE(__m128 &tx, __m128 &ty, const __m128 x2, const __m128 y2, const __m128 r2) {
    __m128 xy = _mm_mul_ps(tx, ty);
    __m128 r4 = _mm_mul_ps(r2, r2);
    __m128 r6 = _mm_mul_ps(r4, r2);

    // float k_jx = 1 + k1 * r2 + k2 * r4 + k3 * r6;
    __m128 k_jx = _mm_add_ps(_mm_set_ps1(1), _mm_add_ps(_mm_add_ps(_mm_mul_ps(color_k1_, r2), _mm_mul_ps(color_k2_, r4)), _mm_mul_ps(color_k3_, r6)));

    // float x_qx = p2 * (2 * x2 + r2) + 2 * p1 * xy;
    __m128 x_qx = _mm_add_ps(_mm_mul_ps(color_p2_, _mm_add_ps(_mm_mul_ps(x2, TWO), r2)), _mm_mul_ps(_mm_mul_ps(color_p1_, xy), TWO));

    // float y_qx = p1 * (2 * y2 + r2) + 2 * p2 * xy;
    __m128 y_qx = _mm_add_ps(_mm_mul_ps(color_p1_, _mm_add_ps(_mm_mul_ps(y2, TWO), r2)), _mm_mul_ps(_mm_mul_ps(color_p2_, xy), TWO));

    // float distx = tx * k_jx + x_qx;
    tx = _mm_add_ps(_mm_mul_ps(tx, k_jx), x_qx);

    // float disty = ty * k_jx + y_qx;
    ty = _mm_add_ps(_mm_mul_ps(ty, k_jx), y_qx);
}

void AlignImpl::BMDistortedWithSSE(__m128 &tx, __m128 &ty, const __m128 x2, const __m128 y2, const __m128 r2) {
    __m128 xy = _mm_mul_ps(tx, ty);
    __m128 r4 = _mm_mul_ps(r2, r2);
    __m128 r6 = _mm_mul_ps(r4, r2);

    // float k_jx = k_diff = (1 + k1 * r2 + k2 * r4 + k3 * r6) / (1 + k4 * r2 + k5 * r4 + k6 * r6);
    __m128 k_jx =
        _mm_div_ps(_mm_add_ps(_mm_set_ps1(1), _mm_add_ps(_mm_add_ps(_mm_mul_ps(color_k1_, r2), _mm_mul_ps(color_k2_, r4)), _mm_mul_ps(color_k3_, r6))),
                   _mm_add_ps(_mm_set_ps1(1), _mm_add_ps(_mm_add_ps(_mm_mul_ps(color_k4_, r2), _mm_mul_ps(color_k5_, r4)), _mm_mul_ps(color_k6_, r6))));

    // float x_qx = p2 * (2 * x2 + r2) + 2 * p1 * xy;
    __m128 x_qx = _mm_add_ps(_mm_mul_ps(color_p2_, _mm_add_ps(_mm_mul_ps(x2, TWO), r2)), _mm_mul_ps(_mm_mul_ps(color_p1_, xy), TWO));

    // float y_qx = p1 * (2 * y2 + r2) + 2 * p2 * xy;
    __m128 y_qx = _mm_add_ps(_mm_mul_ps(color_p1_, _mm_add_ps(_mm_mul_ps(y2, TWO), r2)), _mm_mul_ps(_mm_mul_ps(color_p2_, xy), TWO));

    // float distx = tx * k_jx + x_qx;
    tx = _mm_add_ps(_mm_mul_ps(tx, k_jx), x_qx);

    // float disty = ty * k_jx + y_qx;
    ty = _mm_add_ps(_mm_mul_ps(ty, k_jx), y_qx);
}

void AlignImpl::KBDistortedWithSSE(__m128 &tx, __m128 &ty, const __m128 r2) {
    __m128 r = _mm_sqrt_ps(r2);

    // float theta=atan(r)
    float r_[4]     = { 0 };
    float theta_[4] = { 0 };
    _mm_storeu_ps(r_, r);

    theta_[0] = atan(r_[0]);
    theta_[1] = atan(r_[1]);
    theta_[2] = atan(r_[2]);
    theta_[3] = atan(r_[3]);

    __m128 theta  = _mm_loadu_ps(theta_);
    __m128 theta2 = _mm_mul_ps(theta, theta);
    __m128 theta3 = _mm_mul_ps(theta, theta2);
    __m128 theta5 = _mm_mul_ps(theta2, theta3);
    __m128 theta7 = _mm_mul_ps(theta2, theta5);
    __m128 theta9 = _mm_mul_ps(theta2, theta7);

    // float theta_jx=theta+k1*theta2+k2*theta4+k3*theta6+k4*theta8
    __m128 theta_jx =
        _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_add_ps(theta, _mm_mul_ps(color_k1_, theta3)), _mm_mul_ps(color_k2_, theta5)), _mm_mul_ps(color_k3_, theta7)),
                   _mm_mul_ps(color_k4_, theta9));

    // float tx=(theta_jx/r)*tx
    tx = _mm_mul_ps(_mm_div_ps(theta_jx, r), tx);

    // float ty=(theta_jx/r)*ty
    ty = _mm_mul_ps(_mm_div_ps(theta_jx, r), ty);
}

void AlignImpl::distortedWithoutSSE(const float pt_ud[2], float pt_d[2]) {
    float k1 = rgb_disto_.k1, k2 = rgb_disto_.k2, k3 = rgb_disto_.k3;
    float p1 = rgb_disto_.p1, p2 = rgb_disto_.p2;

    const float r2 = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];

    const float r4 = r2 * r2;
    const float r6 = r4 * r2;

    float k_diff = 1 + k1 * r2 + k2 * r4 + k3 * r6;

    const float t_x = p2 * (r2 + 2 * pt_ud[0] * pt_ud[0]) + 2 * p1 * pt_ud[0] * pt_ud[1];
    const float t_y = p1 * (r2 + 2 * pt_ud[1] * pt_ud[1]) + 2 * p2 * pt_ud[0] * pt_ud[1];

    pt_d[0] = pt_ud[0] * k_diff + t_x;
    pt_d[1] = pt_ud[1] * k_diff + t_y;
}

void AlignImpl::KBDistortedWithoutSSE(const float pt_ud[2], float pt_d[2]) {
    float k1 = rgb_disto_.k1, k2 = rgb_disto_.k2, k3 = rgb_disto_.k3, k4 = rgb_disto_.k4;

    const float r2 = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];

    const double r      = sqrt(r2);
    const double theta  = atan(r);
    const double theta2 = theta * theta;
    const double theta4 = theta2 * theta2;
    const double theta6 = theta2 * theta4;
    const double theta8 = theta4 * theta4;

    const double k_diff = theta * (1 + (k1 * theta2) + (k2 * theta4) + (k3 * theta6) + (k4 * theta8));

    pt_d[0] = static_cast<float>(k_diff / r * pt_ud[0]);
    pt_d[1] = static_cast<float>(k_diff / r * pt_ud[1]);
}

void AlignImpl::BMDistortedWithoutSSE(const float pt_ud[2], float pt_d[2]) {
    float k1 = rgb_disto_.k1, k2 = rgb_disto_.k2, k3 = rgb_disto_.k3;
    float k4 = rgb_disto_.k4, k5 = rgb_disto_.k5, k6 = rgb_disto_.k6;
    float p1 = rgb_disto_.p1, p2 = rgb_disto_.p2;

    const float r2 = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];

    const float r4 = r2 * r2;
    const float r6 = r4 * r2;

    float k_diff = 1 + k1 * r2 + k2 * r4 + k3 * r6;
    k_diff /= (1 + k4 * r2 + k5 * r4 + k6 * r6);

    const float t_x = p2 * (r2 + 2 * pt_ud[0] * pt_ud[0]) + 2 * p1 * pt_ud[0] * pt_ud[1];
    const float t_y = p1 * (r2 + 2 * pt_ud[1] * pt_ud[1]) + 2 * p2 * pt_ud[0] * pt_ud[1];

    pt_d[0] = pt_ud[0] * k_diff + t_x;
    pt_d[1] = pt_ud[1] * k_diff + t_y;
}

void AlignImpl::K3DistortedD2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                         const float *coeff_mat_z[2], int *map) {
    int          channel = (gap_fill_copy_ ? 1 : 2);
    const float *ptr_coeff_x[2];
    const float *ptr_coeff_y[2];
    const float *ptr_coeff_z[2];
    for(int i = 0; i < channel; i++) {
        ptr_coeff_x[i] = coeff_mat_x[i];
        ptr_coeff_y[i] = coeff_mat_y[i];
        ptr_coeff_z[i] = coeff_mat_z[i];
    }
    const uint16_t *ptr_depth = depth_buffer;

    int depth_width  = depth_intric_.width;
    int depth_height = depth_intric_.height;
    int rgb_width    = rgb_intric_.width;
    int rgb_height   = rgb_intric_.height;

    float pixelx_f[2], pixely_f[2], dst[2];

    if(gap_fill_copy_) {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = K3ProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillSingleChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
    else {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = K3ProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillMultiChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
}

void AlignImpl::K6DistortedD2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                         const float *coeff_mat_z[2], int *map) {
    int          channel = (gap_fill_copy_ ? 1 : 2);
    const float *ptr_coeff_x[2];
    const float *ptr_coeff_y[2];
    const float *ptr_coeff_z[2];
    for(int i = 0; i < channel; i++) {
        ptr_coeff_x[i] = coeff_mat_x[i];
        ptr_coeff_y[i] = coeff_mat_y[i];
        ptr_coeff_z[i] = coeff_mat_z[i];
    }
    const uint16_t *ptr_depth = depth_buffer;

    int depth_width  = depth_intric_.width;
    int depth_height = depth_intric_.height;
    int rgb_width    = rgb_intric_.width;
    int rgb_height   = rgb_intric_.height;

    float pixelx_f[2], pixely_f[2], dst[2];

    if(gap_fill_copy_) {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = K6ProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillSingleChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
    else {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = K6ProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillMultiChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
}

void AlignImpl::KBDistortedD2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                         const float *coeff_mat_z[2], int *map) {
    int          channel = (gap_fill_copy_ ? 1 : 2);
    const float *ptr_coeff_x[2];
    const float *ptr_coeff_y[2];
    const float *ptr_coeff_z[2];
    for(int i = 0; i < channel; i++) {
        ptr_coeff_x[i] = coeff_mat_x[i];
        ptr_coeff_y[i] = coeff_mat_y[i];
        ptr_coeff_z[i] = coeff_mat_z[i];
    }
    const uint16_t *ptr_depth = depth_buffer;

    int depth_width  = depth_intric_.width;
    int depth_height = depth_intric_.height;
    int rgb_width    = rgb_intric_.width;
    int rgb_height   = rgb_intric_.height;

    float pixelx_f[2], pixely_f[2], dst[2];

    if(gap_fill_copy_) {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = KBProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillSingleChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
    else {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = KBProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillMultiChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
}

void AlignImpl::LinearDistortedD2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                             const float *coeff_mat_z[2], int *map) {
    int          channel = (gap_fill_copy_ ? 1 : 2);
    const float *ptr_coeff_x[2];
    const float *ptr_coeff_y[2];
    const float *ptr_coeff_z[2];
    for(int i = 0; i < channel; i++) {
        ptr_coeff_x[i] = coeff_mat_x[i];
        ptr_coeff_y[i] = coeff_mat_y[i];
        ptr_coeff_z[i] = coeff_mat_z[i];
    }
    const uint16_t *ptr_depth = depth_buffer;

    int depth_width  = depth_intric_.width;
    int depth_height = depth_intric_.height;
    int rgb_width    = rgb_intric_.width;
    int rgb_height   = rgb_intric_.height;

    float pixelx_f[2], pixely_f[2], dst[2];

    if(gap_fill_copy_) {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = LinearProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillSingleChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
    else {
        for(int v = 0; v < depth_height; v++) {
            int depth_idx = v * depth_width;
            for(int u = 0; u < depth_width; u++) {
                uint16_t depth = *ptr_depth++;
                depth_idx++;

                bool valid = LinearProcessWithoutSSE(depth, ptr_coeff_x, ptr_coeff_y, ptr_coeff_z, channel, pixelx_f, pixely_f, dst);

                if(!valid) {
                    continue;
                }

                FillMultiChannelWithoutSSE(pixelx_f, pixely_f, dst, out_depth, map, depth_idx, rgb_width, rgb_height);
            }
        }
    }
}

inline bool AlignImpl::K3ProcessWithoutSSE(uint16_t depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2], int channel,
                                           float *pixelx_f, float *pixely_f, float *dst) {
    if(depth < EPSILON) {
        for(int i = 0; i < channel; i++) {
            coeff_mat_x[i] += 1;
            coeff_mat_y[i] += 1;
            coeff_mat_z[i] += 1;
        }
        return false;
    }

    bool valid = true;

    for(int k = 0; k < channel; k++) {
        float dst_x = depth * (*coeff_mat_x[k]++) + scaled_trans_[0];
        float dst_y = depth * (*coeff_mat_y[k]++) + scaled_trans_[1];
        float z     = depth * (*coeff_mat_z[k]++) + scaled_trans_[2];

        if(z < EPSILON) {
            valid = false;
            break;
        }

        dst[k]   = z;
        float tx = dst_x / z;
        float ty = dst_y / z;

        float pt_ud[2] = { tx, ty };
        float pt_d[2]  = { 0 };
        distortedWithoutSSE(pt_ud, pt_d);
        tx = pt_d[0];
        ty = pt_d[1];

        pixelx_f[k] = tx * rgb_intric_.fx + rgb_intric_.cx;
        pixely_f[k] = ty * rgb_intric_.fy + rgb_intric_.cy;
    }

    return valid;
}

inline bool AlignImpl::K6ProcessWithoutSSE(uint16_t depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2], int channel,
                                           float *pixelx_f, float *pixely_f, float *dst) {
    if(depth < EPSILON) {
        for(int i = 0; i < channel; i++) {
            coeff_mat_x[i] += 1;
            coeff_mat_y[i] += 1;
            coeff_mat_z[i] += 1;
        }
        return false;
    }

    bool valid = true;

    for(int k = 0; k < channel; k++) {
        float dst_x = depth * (*coeff_mat_x[k]++) + scaled_trans_[0];
        float dst_y = depth * (*coeff_mat_y[k]++) + scaled_trans_[1];
        float z     = depth * (*coeff_mat_z[k]++) + scaled_trans_[2];

        if(z < EPSILON) {
            valid = false;
            break;
        }

        dst[k]   = z;
        float tx = dst_x / z;
        float ty = dst_y / z;

        float pt_ud[2] = { tx, ty };
        float pt_d[2]  = { 0 };
        float r2_cur   = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];

        if((r2_max_loc_ != 0) && (r2_cur > r2_max_loc_)) {
            valid = false;
            break;
        }

        BMDistortedWithoutSSE(pt_ud, pt_d);
        tx = pt_d[0];
        ty = pt_d[1];

        pixelx_f[k] = tx * rgb_intric_.fx + rgb_intric_.cx;
        pixely_f[k] = ty * rgb_intric_.fy + rgb_intric_.cy;
    }

    return valid;
}

inline bool AlignImpl::KBProcessWithoutSSE(uint16_t depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2], int channel,
                                           float *pixelx_f, float *pixely_f, float *dst) {
    if(depth < EPSILON) {
        for(int i = 0; i < channel; i++) {
            coeff_mat_x[i] += 1;
            coeff_mat_y[i] += 1;
            coeff_mat_z[i] += 1;
        }
        return false;
    }

    bool valid = true;

    for(int k = 0; k < channel; k++) {
        float dst_x = depth * (*coeff_mat_x[k]++) + scaled_trans_[0];
        float dst_y = depth * (*coeff_mat_y[k]++) + scaled_trans_[1];
        float z     = depth * (*coeff_mat_z[k]++) + scaled_trans_[2];

        if(z < EPSILON) {
            valid = false;
            break;
        }

        dst[k]   = z;
        float tx = dst_x / z;
        float ty = dst_y / z;

        float pt_ud[2] = { tx, ty };
        float pt_d[2]  = { 0 };
        KBDistortedWithoutSSE(pt_ud, pt_d);
        tx = pt_d[0];
        ty = pt_d[1];

        pixelx_f[k] = tx * rgb_intric_.fx + rgb_intric_.cx;
        pixely_f[k] = ty * rgb_intric_.fy + rgb_intric_.cy;
    }

    return valid;
}

inline bool AlignImpl::LinearProcessWithoutSSE(uint16_t depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2],
                                               int channel, float *pixelx_f, float *pixely_f, float *dst) {
    if(depth < EPSILON) {
        for(int i = 0; i < channel; i++) {
            coeff_mat_x[i] += 1;
            coeff_mat_y[i] += 1;
            coeff_mat_z[i] += 1;
        }
        return false;
    }

    bool valid = true;

    for(int k = 0; k < channel; k++) {
        float dst_x = depth * (*coeff_mat_x[k]++) + scaled_trans_[0];
        float dst_y = depth * (*coeff_mat_y[k]++) + scaled_trans_[1];
        float z     = depth * (*coeff_mat_z[k]++) + scaled_trans_[2];

        if(z < EPSILON) {
            valid = false;
            break;
        }

        dst[k]   = z;
        float tx = dst_x / z;
        float ty = dst_y / z;

        pixelx_f[k] = tx * rgb_intric_.fx + rgb_intric_.cx;
        pixely_f[k] = ty * rgb_intric_.fy + rgb_intric_.cy;
    }

    return valid;
}

void AlignImpl::FillSingleChannelWithoutSSE(const float *pixelx_f, const float *pixely_f, const float *dst, uint16_t *out_depth, int *map, int depth_idx,
                                            int width, int height) {
    int u_rgb = static_cast<int>(pixelx_f[0] + 0.5f);
    int v_rgb = static_cast<int>(pixely_f[0] + 0.5f);

    if(u_rgb >= 0 && u_rgb < width && v_rgb >= 0 && v_rgb < height) {
        if(out_depth) {
            uint16_t cur_depth = static_cast<uint16_t>(dst[0]);
            int      pos       = v_rgb * width + u_rgb;
            out_depth[pos]     = std::min(out_depth[pos], cur_depth);

            if((u_rgb + 1) < width) {
                out_depth[pos + 1] = std::min(out_depth[pos + 1], cur_depth);
                if((v_rgb + 1) < height) {
                    out_depth[pos + width + 1] = std::min(out_depth[pos + width + 1], cur_depth);
                }
            }

            if((v_rgb + 1) < height) {
                out_depth[pos + width] = std::min(out_depth[pos + width], cur_depth);
            }
        }

        if(map) {
            int map_idx      = 2 * (depth_idx - 1);
            map[map_idx]     = u_rgb;
            map[map_idx + 1] = v_rgb;
        }
    }
}

void AlignImpl::FillMultiChannelWithoutSSE(const float *pixelx_f, const float *pixely_f, const float *dst, uint16_t *out_depth, int *map, int depth_idx,
                                           int width, int height) {
    int u_rgb0 = static_cast<int>(pixelx_f[0] + 0.5f);
    int v_rgb0 = static_cast<int>(pixely_f[0] + 0.5f);

    if(out_depth) {
        int u_rgb1 = static_cast<int>(pixelx_f[1] + 0.5f);
        int v_rgb1 = static_cast<int>(pixely_f[1] + 0.5f);
        int u0  = std::max(0, std::min(u_rgb0, u_rgb1));
        int v0     = std::max(0, std::min(v_rgb0, v_rgb1));
        int u1     = std::min(std::max(u_rgb0, u_rgb1), width - 1);
        int v1     = std::min(std::max(v_rgb0, v_rgb1), height - 1);

        uint16_t cur_depth = static_cast<uint16_t>(std::min(dst[0], dst[1]));

        for(int vr = v0; vr <= v1; vr++) {
            int row_start = vr * width;
            for(int ur = u0; ur <= u1; ur++) {
                int pos        = row_start + ur;
                out_depth[pos] = std::min(out_depth[pos], cur_depth);
            }
        }
    }

    if(map) {
        if((u_rgb0 >= 0) && (u_rgb0 < width) && (v_rgb0 >= 0) && (v_rgb0 < height)) {
            int map_idx      = 2 * (depth_idx - 1);
            map[map_idx]     = u_rgb0;
            map[map_idx + 1] = v_rgb0;
        }
    }
}

void AlignImpl::K3DistortedD2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                      const float *coeff_mat_z[2], int *map) {
    int channel      = (gap_fill_copy_ ? 1 : 2);
    int total_pixels = depth_intric_.width * depth_intric_.height;
    int width        = rgb_intric_.width;
    int height       = rgb_intric_.height;

    float x_lo[8] = { 0 };
    float y_lo[8] = { 0 };
    float z_lo[8] = { 0 };
    float x_hi[8] = { 0 };
    float y_hi[8] = { 0 };
    float z_hi[8] = { 0 };

    // center
    if(gap_fill_copy_) {
        // processing full chunks of 8 pixels
        for(int i = 0; i < total_pixels; i += 8) {
            K3ProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillSingleChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillSingleChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
    else {  // top - left - and-bottom - right
        for(int i = 0; i < total_pixels; i += 8) {
            K3ProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillMultiChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillMultiChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
}

void AlignImpl::K6DistortedD2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                      const float *coeff_mat_z[2], int *map) {
    int channel      = (gap_fill_copy_ ? 1 : 2);
    int total_pixels = depth_intric_.width * depth_intric_.height;
    int width        = rgb_intric_.width;
    int height       = rgb_intric_.height;

    float x_lo[8] = { 0 };
    float y_lo[8] = { 0 };
    float z_lo[8] = { 0 };
    float x_hi[8] = { 0 };
    float y_hi[8] = { 0 };
    float z_hi[8] = { 0 };

    // center
    if(gap_fill_copy_) {
        // processing full chunks of 8 pixels
        for(int i = 0; i < total_pixels; i += 8) {
            K6ProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillSingleChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillSingleChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
    else {  // top - left - and-bottom - right
        for(int i = 0; i < total_pixels; i += 8) {
            K6ProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillMultiChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillMultiChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
}

void AlignImpl::KBDistortedD2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                      const float *coeff_mat_z[2], int *map) {
    int channel      = (gap_fill_copy_ ? 1 : 2);
    int total_pixels = depth_intric_.width * depth_intric_.height;
    int width        = rgb_intric_.width;
    int height       = rgb_intric_.height;

    float x_lo[8] = { 0 };
    float y_lo[8] = { 0 };
    float z_lo[8] = { 0 };
    float x_hi[8] = { 0 };
    float y_hi[8] = { 0 };
    float z_hi[8] = { 0 };

    // center
    if(gap_fill_copy_) {
        // processing full chunks of 8 pixels
        for(int i = 0; i < total_pixels; i += 8) {
            KBProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillSingleChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillSingleChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
    else {  // top - left - and-bottom - right
        for(int i = 0; i < total_pixels; i += 8) {
            KBProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillMultiChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillMultiChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
}

void AlignImpl::LinearD2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x[2], const float *coeff_mat_y[2],
                                 const float *coeff_mat_z[2], int *map) {
    int channel      = (gap_fill_copy_ ? 1 : 2);
    int total_pixels = depth_intric_.width * depth_intric_.height;
    int width        = rgb_intric_.width;
    int height       = rgb_intric_.height;

    float x_lo[8] = { 0 };
    float y_lo[8] = { 0 };
    float z_lo[8] = { 0 };
    float x_hi[8] = { 0 };
    float y_hi[8] = { 0 };
    float z_hi[8] = { 0 };

    // center
    if(gap_fill_copy_) {
        // processing full chunks of 8 pixels
        for(int i = 0; i < total_pixels; i += 8) {
            LinearProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillSingleChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillSingleChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
    else {  // top - left - and-bottom - right
        for(int i = 0; i < total_pixels; i += 8) {
            LinearProcessWithSSE(depth_buffer, coeff_mat_x, coeff_mat_y, coeff_mat_z, x_lo, y_lo, z_lo, x_hi, y_hi, z_hi, i, channel);

            FillMultiChannelWithSSE(x_lo, y_lo, z_lo, out_depth, map, i, width, height);
            FillMultiChannelWithSSE(x_hi, y_hi, z_hi, out_depth, map, i + 4, width, height);
        }
    }
}

inline void AlignImpl::K3ProcessWithSSE(const uint16_t *depth_buffer, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2],
                                        float *x_lo, float *y_lo, float *z_lo, float *x_hi, float *y_hi, float *z_hi, int start_idx, int channel) {
    __m128i depth_i16  = _mm_loadu_si128((__m128i *)(depth_buffer + start_idx));
    __m128i depth_i_lo = _mm_unpacklo_epi16(depth_i16, ZERO);
    __m128i depth_i_hi = _mm_unpackhi_epi16(depth_i16, ZERO);

    __m128 depth_sse_lo = _mm_cvtepi32_ps(depth_i_lo);
    __m128 depth_sse_hi = _mm_cvtepi32_ps(depth_i_hi);

    for(int fold = 0; fold < channel; fold++) {
        __m128 coeff_sse1_lo = _mm_loadu_ps(coeff_mat_x[fold] + start_idx);
        __m128 coeff_sse1_hi = _mm_loadu_ps(coeff_mat_x[fold] + start_idx + 4);
        __m128 coeff_sse2_lo = _mm_loadu_ps(coeff_mat_y[fold] + start_idx);
        __m128 coeff_sse2_hi = _mm_loadu_ps(coeff_mat_y[fold] + start_idx + 4);
        __m128 coeff_sse3_lo = _mm_loadu_ps(coeff_mat_z[fold] + start_idx);
        __m128 coeff_sse3_hi = _mm_loadu_ps(coeff_mat_z[fold] + start_idx + 4);

        __m128 depth_o_lo, nx_lo, ny_lo;
        __m128 depth_o_hi, nx_hi, ny_hi;
        CalcNormCorrdWithSSE(depth_sse_lo, coeff_sse1_lo, coeff_sse2_lo, coeff_sse3_lo, depth_o_lo, nx_lo, ny_lo);
        CalcNormCorrdWithSSE(depth_sse_hi, coeff_sse1_hi, coeff_sse2_hi, coeff_sse3_hi, depth_o_hi, nx_hi, ny_hi);

        __m128 x2_lo = _mm_mul_ps(nx_lo, nx_lo);
        __m128 y2_lo = _mm_mul_ps(ny_lo, ny_lo);
        __m128 r2_lo = _mm_add_ps(x2_lo, y2_lo);
        __m128 x2_hi = _mm_mul_ps(nx_hi, nx_hi);
        __m128 y2_hi = _mm_mul_ps(ny_hi, ny_hi);
        __m128 r2_hi = _mm_add_ps(x2_hi, y2_hi);

        distortedWithSSE(nx_lo, ny_lo, x2_lo, y2_lo, r2_lo);
        distortedWithSSE(nx_hi, ny_hi, x2_hi, y2_hi, r2_hi);

        __m128 pixelx_lo = _mm_add_ps(_mm_mul_ps(nx_lo, color_fx_), color_cx_);
        __m128 pixely_lo = _mm_add_ps(_mm_mul_ps(ny_lo, color_fy_), color_cy_);
        __m128 pixelx_hi = _mm_add_ps(_mm_mul_ps(nx_hi, color_fx_), color_cx_);
        __m128 pixely_hi = _mm_add_ps(_mm_mul_ps(ny_hi, color_fy_), color_cy_);

        _mm_storeu_ps(x_lo + fold * 4, pixelx_lo);
        _mm_storeu_ps(y_lo + fold * 4, pixely_lo);
        _mm_storeu_ps(z_lo + fold * 4, depth_o_lo);
        _mm_storeu_ps(x_hi + fold * 4, pixelx_hi);
        _mm_storeu_ps(y_hi + fold * 4, pixely_hi);
        _mm_storeu_ps(z_hi + fold * 4, depth_o_hi);
    }
}

inline void AlignImpl::K6ProcessWithSSE(const uint16_t *depth_buffer, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2],
                                        float *x_lo, float *y_lo, float *z_lo, float *x_hi, float *y_hi, float *z_hi, int start_idx, int channel) {
    __m128i depth_i16  = _mm_loadu_si128((__m128i *)(depth_buffer + start_idx));
    __m128i depth_i_lo = _mm_unpacklo_epi16(depth_i16, ZERO);
    __m128i depth_i_hi = _mm_unpackhi_epi16(depth_i16, ZERO);

    __m128 depth_sse_lo = _mm_cvtepi32_ps(depth_i_lo);
    __m128 depth_sse_hi = _mm_cvtepi32_ps(depth_i_hi);

    for(int fold = 0; fold < channel; fold++) {
        __m128 coeff_sse1_lo = _mm_loadu_ps(coeff_mat_x[fold] + start_idx);
        __m128 coeff_sse1_hi = _mm_loadu_ps(coeff_mat_x[fold] + start_idx + 4);
        __m128 coeff_sse2_lo = _mm_loadu_ps(coeff_mat_y[fold] + start_idx);
        __m128 coeff_sse2_hi = _mm_loadu_ps(coeff_mat_y[fold] + start_idx + 4);
        __m128 coeff_sse3_lo = _mm_loadu_ps(coeff_mat_z[fold] + start_idx);
        __m128 coeff_sse3_hi = _mm_loadu_ps(coeff_mat_z[fold] + start_idx + 4);

        __m128 depth_o_lo, nx_lo, ny_lo;
        __m128 depth_o_hi, nx_hi, ny_hi;
        CalcNormCorrdWithSSE(depth_sse_lo, coeff_sse1_lo, coeff_sse2_lo, coeff_sse3_lo, depth_o_lo, nx_lo, ny_lo);
        CalcNormCorrdWithSSE(depth_sse_hi, coeff_sse1_hi, coeff_sse2_hi, coeff_sse3_hi, depth_o_hi, nx_hi, ny_hi);

        __m128 x2_lo = _mm_mul_ps(nx_lo, nx_lo);
        __m128 y2_lo = _mm_mul_ps(ny_lo, ny_lo);
        __m128 r2_lo = _mm_add_ps(x2_lo, y2_lo);
        __m128 x2_hi = _mm_mul_ps(nx_hi, nx_hi);
        __m128 y2_hi = _mm_mul_ps(ny_hi, ny_hi);
        __m128 r2_hi = _mm_add_ps(x2_hi, y2_hi);

        __m128 flag_lo = _mm_or_ps(_mm_cmpge_ps(ZERO_F, r2_max_loc_sse_), _mm_cmplt_ps(r2_lo, r2_max_loc_sse_));
        __m128 flag_hi = _mm_or_ps(_mm_cmpge_ps(ZERO_F, r2_max_loc_sse_), _mm_cmplt_ps(r2_hi, r2_max_loc_sse_));

        depth_o_lo = _mm_and_ps(depth_o_lo, flag_lo);
        BMDistortedWithSSE(nx_lo, ny_lo, x2_lo, y2_lo, r2_lo);
        depth_o_hi = _mm_and_ps(depth_o_hi, flag_hi);
        BMDistortedWithSSE(nx_hi, ny_hi, x2_hi, y2_hi, r2_lo);

        __m128 pixelx_lo = _mm_add_ps(_mm_mul_ps(nx_lo, color_fx_), color_cx_);
        __m128 pixely_lo = _mm_add_ps(_mm_mul_ps(ny_lo, color_fy_), color_cy_);
        __m128 pixelx_hi = _mm_add_ps(_mm_mul_ps(nx_hi, color_fx_), color_cx_);
        __m128 pixely_hi = _mm_add_ps(_mm_mul_ps(ny_hi, color_fy_), color_cy_);

        _mm_storeu_ps(x_lo + fold * 4, pixelx_lo);
        _mm_storeu_ps(y_lo + fold * 4, pixely_lo);
        _mm_storeu_ps(z_lo + fold * 4, depth_o_lo);
        _mm_storeu_ps(x_hi + fold * 4, pixelx_hi);
        _mm_storeu_ps(y_hi + fold * 4, pixely_hi);
        _mm_storeu_ps(z_hi + fold * 4, depth_o_hi);
    }
}

inline void AlignImpl::KBProcessWithSSE(const uint16_t *depth_buffer, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2],
                                        float *x_lo, float *y_lo, float *z_lo, float *x_hi, float *y_hi, float *z_hi, int start_idx, int channel) {
    __m128i depth_i16  = _mm_loadu_si128((__m128i *)(depth_buffer + start_idx));
    __m128i depth_i_lo = _mm_unpacklo_epi16(depth_i16, ZERO);
    __m128i depth_i_hi = _mm_unpackhi_epi16(depth_i16, ZERO);

    __m128 depth_sse_lo = _mm_cvtepi32_ps(depth_i_lo);
    __m128 depth_sse_hi = _mm_cvtepi32_ps(depth_i_hi);

    for(int fold = 0; fold < channel; fold++) {
        __m128 coeff_sse1_lo = _mm_loadu_ps(coeff_mat_x[fold] + start_idx);
        __m128 coeff_sse1_hi = _mm_loadu_ps(coeff_mat_x[fold] + start_idx + 4);
        __m128 coeff_sse2_lo = _mm_loadu_ps(coeff_mat_y[fold] + start_idx);
        __m128 coeff_sse2_hi = _mm_loadu_ps(coeff_mat_y[fold] + start_idx + 4);
        __m128 coeff_sse3_lo = _mm_loadu_ps(coeff_mat_z[fold] + start_idx);
        __m128 coeff_sse3_hi = _mm_loadu_ps(coeff_mat_z[fold] + start_idx + 4);

        __m128 depth_o_lo, nx_lo, ny_lo;
        __m128 depth_o_hi, nx_hi, ny_hi;
        CalcNormCorrdWithSSE(depth_sse_lo, coeff_sse1_lo, coeff_sse2_lo, coeff_sse3_lo, depth_o_lo, nx_lo, ny_lo);
        CalcNormCorrdWithSSE(depth_sse_hi, coeff_sse1_hi, coeff_sse2_hi, coeff_sse3_hi, depth_o_hi, nx_hi, ny_hi);

        __m128 x2_lo = _mm_mul_ps(nx_lo, nx_lo);
        __m128 y2_lo = _mm_mul_ps(ny_lo, ny_lo);
        __m128 r2_lo = _mm_add_ps(x2_lo, y2_lo);
        __m128 x2_hi = _mm_mul_ps(nx_hi, nx_hi);
        __m128 y2_hi = _mm_mul_ps(ny_hi, ny_hi);
        __m128 r2_hi = _mm_add_ps(x2_hi, y2_hi);

        KBDistortedWithSSE(nx_lo, ny_lo, r2_lo);
        KBDistortedWithSSE(nx_hi, ny_hi, r2_hi);

        __m128 pixelx_lo = _mm_add_ps(_mm_mul_ps(nx_lo, color_fx_), color_cx_);
        __m128 pixely_lo = _mm_add_ps(_mm_mul_ps(ny_lo, color_fy_), color_cy_);
        __m128 pixelx_hi = _mm_add_ps(_mm_mul_ps(nx_hi, color_fx_), color_cx_);
        __m128 pixely_hi = _mm_add_ps(_mm_mul_ps(ny_hi, color_fy_), color_cy_);

        _mm_storeu_ps(x_lo + fold * 4, pixelx_lo);
        _mm_storeu_ps(y_lo + fold * 4, pixely_lo);
        _mm_storeu_ps(z_lo + fold * 4, depth_o_lo);
        _mm_storeu_ps(x_hi + fold * 4, pixelx_hi);
        _mm_storeu_ps(y_hi + fold * 4, pixely_hi);
        _mm_storeu_ps(z_hi + fold * 4, depth_o_hi);
    }
}

inline void AlignImpl::LinearProcessWithSSE(const uint16_t *depth_buffer, const float *coeff_mat_x[2], const float *coeff_mat_y[2], const float *coeff_mat_z[2],
                                            float *x_lo, float *y_lo, float *z_lo, float *x_hi, float *y_hi, float *z_hi, int start_idx, int channel) {
    __m128i depth_i16  = _mm_loadu_si128((__m128i *)(depth_buffer + start_idx));
    __m128i depth_i_lo = _mm_unpacklo_epi16(depth_i16, ZERO);
    __m128i depth_i_hi = _mm_unpackhi_epi16(depth_i16, ZERO);

    __m128 depth_sse_lo = _mm_cvtepi32_ps(depth_i_lo);
    __m128 depth_sse_hi = _mm_cvtepi32_ps(depth_i_hi);

    for(int fold = 0; fold < channel; fold++) {
        __m128 coeff_sse1_lo = _mm_loadu_ps(coeff_mat_x[fold] + start_idx);
        __m128 coeff_sse1_hi = _mm_loadu_ps(coeff_mat_x[fold] + start_idx + 4);
        __m128 coeff_sse2_lo = _mm_loadu_ps(coeff_mat_y[fold] + start_idx);
        __m128 coeff_sse2_hi = _mm_loadu_ps(coeff_mat_y[fold] + start_idx + 4);
        __m128 coeff_sse3_lo = _mm_loadu_ps(coeff_mat_z[fold] + start_idx);
        __m128 coeff_sse3_hi = _mm_loadu_ps(coeff_mat_z[fold] + start_idx + 4);

        __m128 depth_o_lo, nx_lo, ny_lo;
        __m128 depth_o_hi, nx_hi, ny_hi;
        CalcNormCorrdWithSSE(depth_sse_lo, coeff_sse1_lo, coeff_sse2_lo, coeff_sse3_lo, depth_o_lo, nx_lo, ny_lo);
        CalcNormCorrdWithSSE(depth_sse_hi, coeff_sse1_hi, coeff_sse2_hi, coeff_sse3_hi, depth_o_hi, nx_hi, ny_hi);

        __m128 pixelx_lo = _mm_add_ps(_mm_mul_ps(nx_lo, color_fx_), color_cx_);
        __m128 pixely_lo = _mm_add_ps(_mm_mul_ps(ny_lo, color_fy_), color_cy_);
        __m128 pixelx_hi = _mm_add_ps(_mm_mul_ps(nx_hi, color_fx_), color_cx_);
        __m128 pixely_hi = _mm_add_ps(_mm_mul_ps(ny_hi, color_fy_), color_cy_);

        _mm_storeu_ps(x_lo + fold * 4, pixelx_lo);
        _mm_storeu_ps(y_lo + fold * 4, pixely_lo);
        _mm_storeu_ps(z_lo + fold * 4, depth_o_lo);
        _mm_storeu_ps(x_hi + fold * 4, pixelx_hi);
        _mm_storeu_ps(y_hi + fold * 4, pixely_hi);
        _mm_storeu_ps(z_hi + fold * 4, depth_o_hi);
    }
}

void AlignImpl::CalcNormCorrdWithSSE(const __m128 &depth_sse, const __m128 &coeff_sse1, const __m128 &coeff_sse2, const __m128 &coeff_sse3, __m128 &depth_o,
                                     __m128 &nx, __m128 &ny) {
    __m128 Y = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse2), scaled_trans_2_);
    __m128 X = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse1), scaled_trans_1_);
    depth_o  = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse3), scaled_trans_3_);

    nx = _mm_div_ps(X, depth_o);
    ny = _mm_div_ps(Y, depth_o);
}

inline void AlignImpl::FillSingleChannelWithSSE(const float *x, const float *y, const float *z, uint16_t *out_depth, int *map, int start_idx, int width,
                                                int height) {
    for(int j = 0; j < 4; j++) {
        if(z[j] < EPSILON)
            continue;

        int u_rgb = static_cast<int>(x[j] + 0.5f);
        int v_rgb = static_cast<int>(y[j] + 0.5f);

        if((u_rgb >= 0) && (u_rgb < width) && (v_rgb >= 0) && (v_rgb < height)) {
            if(out_depth) {
                uint16_t cur_depth = static_cast<uint16_t>(z[j]);
                int      pos       = v_rgb * width + u_rgb;
                out_depth[pos]     = std::min(out_depth[pos], cur_depth);
                if((u_rgb + 1) < width) {
                    out_depth[pos + 1] = std::min(out_depth[pos + 1], cur_depth);
                    if((v_rgb + 1) < height) {
                        out_depth[pos + width + 1] = std::min(out_depth[pos + width + 1], cur_depth);
                    }
                }

                if((v_rgb + 1) < height) {
                    out_depth[pos + width] = std::min(out_depth[pos + width], cur_depth);
                }
            }

            if(map) {  // coordinates mapping for C2D
                int map_idx      = 2 * (start_idx + j);
                map[map_idx]     = u_rgb;
                map[map_idx + 1] = v_rgb;
            }
        }
    }
}

inline void AlignImpl::FillMultiChannelWithSSE(const float *x, const float *y, const float *z, uint16_t *out_depth, int *map, int start_idx, int width,
                                               int height) {
    for(int j = 0; j < 4; j++) {
        bool     valid     = true;
        int      u_rgb[2]  = { -1, -1 };
        int      v_rgb[2]  = { -1, -1 };
        uint16_t cur_depth = 65535;

        for(int chl = 0; chl < 2; chl++) {
            int m = j + chl * 4;
            if(z[m] < EPSILON) {
                valid = false;
                break;
            }

            u_rgb[chl] = static_cast<int>(x[m] + 0.5);
            v_rgb[chl] = static_cast<int>(y[m] + 0.5);
            cur_depth  = std::min(cur_depth, static_cast<uint16_t>(z[m]));
        }

        if(!valid)
            continue;

        if(out_depth) {
            int u0 = std::max(0, std::min(u_rgb[0], u_rgb[1]));
            int v0 = std::max(0, std::min(v_rgb[0], v_rgb[1]));
            int u1 = std::min(std::max(u_rgb[0], u_rgb[1]), width - 1);
            int v1 = std::min(std::max(v_rgb[0], v_rgb[1]), height - 1);

            for(int vr = v0; vr <= v1; vr++) {
                int row_start = vr * width;
                for(int ur = u0; ur <= u1; ur++) {
                    int pos        = row_start + ur;
                    out_depth[pos] = std::min(out_depth[pos], cur_depth);
                }
            }
        }

        if(map) {
            if((u_rgb[0] >= 0) && (u_rgb[0] < width) && (v_rgb[0] >= 0) && (v_rgb[0] < height)) {
                int map_idx      = 2 * (start_idx + j);
                map[map_idx]     = u_rgb[0];
                map[map_idx + 1] = v_rgb[0];
            }
        }
    }
}

void AlignImpl::D2CPostProcess(const uint16_t *ptr_src, const int in_width, const int in_height, const float scale, uint16_t *ptr_dst, int out_width,
                               int out_height) {

    // int dst_w = static_cast<int>(in_width * scale);
    // int dst_h = static_cast<int>(in_height * scale);

    unsigned int xrIntFloat_16 = static_cast<unsigned int>(powf(2, 16) / scale + 1);
    unsigned int yrIntFloat_16 = xrIntFloat_16;
    memset(ptr_dst, 0, sizeof(uint16_t) * out_width * out_height);

    int y_end   = static_cast<int>(in_height * scale);
    int x_end   = static_cast<int>(in_width * scale);
    int y_start = 0;
    int x_start = 0;

    int          y0 = 0, x0 = 0;
    uint16_t    *ptr_dst_line = ptr_dst + y_start * out_width;
    unsigned int src_y_16     = y0 << 16;

    for(int y = y_start; y < y_end; ++y) {
        int             y_src        = (src_y_16 >> 16);
        int             y_pix_src    = y_src * in_width;
        const uint16_t *ptr_src_line = ptr_src + y_pix_src;
        unsigned int    src_x_16     = x0 << 16;
        for(int x = x_start; x < x_end; ++x) {
            *(ptr_dst_line + x) = *(ptr_src_line + (src_x_16 >> 16));
            src_x_16 += xrIntFloat_16;
        }
        src_y_16 += yrIntFloat_16;
        ptr_dst_line += out_width;
    }
}

int AlignImpl::D2C(const uint16_t *depth_buffer, int depth_width, int depth_height, uint16_t *out_depth, int color_width, int color_height, int *map,
                   bool withSSE) {
    int ret = 0;
    if(!initialized_ || depth_width != depth_intric_.width || depth_height != depth_intric_.height || color_width != rgb_intric_.width
       || color_height != rgb_intric_.height) {
        LOG_ERROR("Not initialized or input parameters don't match");
        return -1;
    }

    auto finder_x = rot_coeff_ht_x.find(std::make_pair(depth_width, depth_height));
    auto finder_y = rot_coeff_ht_y.find(std::make_pair(depth_width, depth_height));
    auto finder_z = rot_coeff_ht_z.find(std::make_pair(depth_width, depth_height));
    if((rot_coeff_ht_x.cend() == finder_x) || (rot_coeff_ht_y.cend() == finder_y) || (rot_coeff_ht_z.cend() == finder_z)) {
        LOG_ERROR("Found a new resolution, but initialization failed!");
        return -1;
    }

    if((!depth_buffer) || ((!out_depth && !map))) {
        LOG_ERROR("Buffer not initialized");
        return -1;
    }

    float    scale           = 0;
    float    rgb_temp_fx     = 0.0f;
    float    rgb_temp_fy     = 0.0f;
    float    rgb_temp_cx     = 0.0f;
    float    rgb_temp_cy     = 0.0f;
    uint16_t rgb_temp_width  = 0;
    uint16_t rgb_temp_height = 0;
    if(use_scale_) {
        scale              = rgb_intric_.fx / depth_intric_.fx;
        rgb_temp_fx        = rgb_intric_.fx;
        rgb_temp_fy        = rgb_intric_.fy;
        rgb_temp_cx        = rgb_intric_.cx;
        rgb_temp_cy        = rgb_intric_.cy;
        rgb_temp_width     = rgb_intric_.width;
        rgb_temp_height    = rgb_intric_.height;
        rgb_intric_.fx     = rgb_intric_.fx / scale;
        rgb_intric_.fy     = rgb_intric_.fy / scale;
        rgb_intric_.cx     = rgb_intric_.cx / scale;
        rgb_intric_.cy     = rgb_intric_.cy / scale;
        rgb_intric_.width  = static_cast<int16_t>(rgb_intric_.width / scale);
        rgb_intric_.height = static_cast<int16_t>(rgb_intric_.height / scale);
        color_fx_          = _mm_set_ps1(rgb_intric_.fx);
        color_cx_          = _mm_set_ps1(rgb_intric_.cx);
        color_fy_          = _mm_set_ps1(rgb_intric_.fy);
        color_cy_          = _mm_set_ps1(rgb_intric_.cy);
    }

    int pixnum = rgb_intric_.width * rgb_intric_.height;
    if(out_depth) {
        // init to 1s (depth 0 may be used as other useful date)
        memset(out_depth, 0xff, pixnum * sizeof(uint16_t));
    }
    const float *coeff_mat_x[2];
    const float *coeff_mat_y[2];
    const float *coeff_mat_z[2];
    for(int i = 0; i < 2; i++) {
        coeff_mat_x[i] = finder_x->second[i];
        coeff_mat_y[i] = finder_y->second[i];
        coeff_mat_z[i] = finder_z->second[i];
    }

    if(withSSE) {
        if(add_target_distortion_) {
            switch(rgb_disto_.model) {
            case OB_DISTORTION_BROWN_CONRADY:
                K3DistortedD2CWithSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            case OB_DISTORTION_BROWN_CONRADY_K6:
                K6DistortedD2CWithSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            case OB_DISTORTION_KANNALA_BRANDT4:
                KBDistortedD2CWithSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            default:
                LOG_ERROR("Distortion model not supported yet");
                break;
            }
        }
        else {
            LinearD2CWithSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
        }
    }
    else {
        if(add_target_distortion_) {
            switch(rgb_disto_.model) {
            case OB_DISTORTION_BROWN_CONRADY:
                K3DistortedD2CWithoutSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            case OB_DISTORTION_BROWN_CONRADY_K6:
                K6DistortedD2CWithoutSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            case OB_DISTORTION_KANNALA_BRANDT4:
                KBDistortedD2CWithoutSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
                break;
            default:
                LOG_ERROR("Distortion model not supported yet");
                break;
            }
        }
        else {
            LinearDistortedD2CWithoutSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
        }
    }

    if(use_scale_) {
        if(out_depth) {
            uint16_t *out_depth_dst = (uint16_t *)malloc(pixnum * sizeof(uint16_t));
            memcpy(out_depth_dst, out_depth, pixnum * sizeof(uint16_t));
            int pixnumutemp = rgb_temp_width * rgb_temp_height;
            memset(out_depth, 0xff, pixnumutemp * sizeof(uint16_t));
            D2CPostProcess(out_depth_dst, rgb_intric_.width, rgb_intric_.height, scale, out_depth, rgb_temp_width, rgb_temp_height);
            free(out_depth_dst);
        }

        rgb_intric_.fx     = rgb_temp_fx;
        rgb_intric_.fy     = rgb_temp_fy;
        rgb_intric_.cx     = rgb_temp_cx;
        rgb_intric_.cy     = rgb_temp_cy;
        rgb_intric_.width  = rgb_temp_width;
        rgb_intric_.height = rgb_temp_height;
        color_fx_          = _mm_set_ps1(rgb_intric_.fx);
        color_cx_          = _mm_set_ps1(rgb_intric_.cx);
        color_fy_          = _mm_set_ps1(rgb_intric_.fy);
        color_cy_          = _mm_set_ps1(rgb_intric_.cy);
    }
    pixnum = rgb_intric_.width * rgb_intric_.height;
    if(out_depth) {
        for(int idx = 0; idx < pixnum; idx++) {
            if(65535 == out_depth[idx]) {
                out_depth[idx] = 0;
            }
        }
    }

    return ret;
}

typedef struct {
    unsigned char byte[3];
} uint24_t;

int AlignImpl::C2D(const uint16_t *depth_buffer, int depth_width, int depth_height, const void *rgb_buffer, void *out_rgb, int color_width, int color_height,
                   OBFormat format, bool withSSE) {

    // rgb x-y coordinates for each depth pixel
    unsigned long long size     = static_cast<unsigned long long>(depth_width) * depth_height * 2;
    int               *depth_xy = new int[size];
    memset(depth_xy, -1, size * sizeof(int));

    int ret = -1;
    if(!D2C(depth_buffer, depth_width, depth_height, nullptr, color_width, color_height, depth_xy, withSSE)) {

        switch(format) {
        case OB_FORMAT_Y8:
            memset(out_rgb, 0, depth_width * depth_height * sizeof(uint8_t));
            mapPixel<uint8_t>(depth_xy, static_cast<const uint8_t *>(rgb_buffer), color_width, color_height, (uint8_t *)out_rgb, depth_width, depth_height);
            break;
        case OB_FORMAT_Y16:
            memset(out_rgb, 0, depth_width * depth_height * sizeof(uint16_t));
            mapPixel<uint16_t>(depth_xy, static_cast<const uint16_t *>(rgb_buffer), color_width, color_height, (uint16_t *)out_rgb, depth_width, depth_height);
            break;
        case OB_FORMAT_RGB:
        case OB_FORMAT_BGR:
            memset(out_rgb, 0, depth_width * depth_height * sizeof(uint24_t));
            mapPixel<uint24_t>(depth_xy, static_cast<const uint24_t *>(rgb_buffer), color_width, color_height, (uint24_t *)out_rgb, depth_width, depth_height);
            break;
        case OB_FORMAT_BGRA:
        case OB_FORMAT_RGBA:
            memset(out_rgb, 0, depth_width * depth_height * sizeof(uint32_t));
            mapPixel<uint32_t>(depth_xy, static_cast<const uint32_t *>(rgb_buffer), color_width, color_height, (uint32_t *)out_rgb, depth_width, depth_height);
            break;
        case OB_FORMAT_MJPG:
        case OB_FORMAT_YUYV:
        default:
            LOG_ERROR("Unsupported format for C2D conversion yet!");
            break;
        }

        ret = 0;
    }
    delete[] depth_xy;
    depth_xy = nullptr;

    return ret;
}

template <typename T>
void AlignImpl::mapPixel(const int *map, const T *src_buffer, int src_width, int src_height, T *dst_buffer, int dst_width, int dst_height) {
    for(int v = 0; v < dst_height; v++) {
        for(int u = 0; u < dst_width; u++) {
            int id = v * dst_width + u;
            int us = map[2 * id], vs = map[2 * id + 1];
            if((us < 0) || (us > src_width - 1) || (vs < 0) || (vs > src_height - 1))
                continue;
            int is         = vs * src_width + us;
            dst_buffer[id] = src_buffer[is];
        }
    }
}

}  // namespace libobsensor