#include "AlignImpl.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include <fstream>
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
    depth_unit_mm_ = 1.0;
    r2_max_loc_    = 0.0;
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
                           OBExtrinsic extrin, float depth_unit_mm, bool add_target_distortion, bool gap_fill_copy) {
    if(initialized_) {
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
    initialized_ = true;
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
			float r2[2];
			ob_camera_intrinsic intrs[] = { depth_intr, rgb_intr };
			for(int i = 0; i < 2; i++) {
				ob_camera_intrinsic intr = intrs[i];
				float u = (intr.cx > (intr.width - intr.cx)) ? intr.cx : (intr.width - intr.cx),
					  v = (intr.cy > (intr.height - intr.cy)) ? intr.cy : (intr.height - intr.cy);
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
    {
        int                channel   = (gap_fill_copy_ ? 1 : 2);
        unsigned long long stride    = depth_intric_.width * channel;
        unsigned long long coeff_num = depth_intric_.height * stride;

        float *rot_coeff1 = new float[coeff_num];
        float *rot_coeff2 = new float[coeff_num];
        float *rot_coeff3 = new float[coeff_num];

        for(int i = 0; i < channel; i++) {
            int mutliplier = (gap_fill_copy_ ? 0 : (i ? 1 : -1));
            for(int v = 0; v < depth_intric_.height; ++v) {
                float *dst1 = rot_coeff1 + v * stride + i;
                float *dst2 = rot_coeff2 + v * stride + i;
                float *dst3 = rot_coeff3 + v * stride + i;
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
                    dst1 += channel;
                    dst2 += channel;
                    dst3 += channel;
                }
            }
        }

        rot_coeff_ht_x[std::make_pair(depth_intric_.width, depth_intric_.height)] = rot_coeff1;
        rot_coeff_ht_y[std::make_pair(depth_intric_.width, depth_intric_.height)] = rot_coeff2;
        rot_coeff_ht_z[std::make_pair(depth_intric_.width, depth_intric_.height)] = rot_coeff3;
    }
}

void AlignImpl::clearMatrixCache() {
    for(auto item: rot_coeff_ht_x) {
        if(item.second) {
            delete[] item.second;
        }
    }
    for(auto item: rot_coeff_ht_y) {
        if(item.second) {
            delete[] item.second;
        }
    }
    for(auto item: rot_coeff_ht_z) {
        if(item.second) {
            delete[] item.second;
        }
    }

    rot_coeff_ht_x.clear();
    rot_coeff_ht_y.clear();
    rot_coeff_ht_z.clear();
}

template <typename T> void fillPixelGap(const int *u, const int *v, const int width, const int height, const T val, T *buffer, bool copy = true) {
    // point index and output depth buffer should be checked outside

    if(copy) {
        if((u[0] >= 0) && (u[0] < width) && (v[0] >= 0) && (v[0] < height)) {
            int pos          = v[0] * width + u[0];
            buffer[pos]      = val;
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
        int v0 = v[0] < 0 ? 0 : v[0];
        int u0 = u[0] < 0 ? 0 : u[0];
        int v1 = v[1] < (height - 1) ? v[1] : (height - 1);
        int u1 = u[1] < (width - 1) ? u[1] : (width - 1);
        for(int vr = v0; vr <= v1; vr++) {
            for(int ur = u0; ur <= u1; ur++) {
                int pos = vr * width + ur;
                if(buffer[pos] > val)
                    buffer[pos] = val;
            }
        }
    }
}

void AlignImpl::transferDepth(const float *x, const float *y, const float *z, const int npts, const int point_index, uint16_t *out_depth, int *map) {
    int nchannels = gap_fill_copy_ ? 1 : 2;

    for(int i = 0; i < npts; i++) {

        int u_rgb[] = { -1, -1 };
        int v_rgb[] = { -1, -1 };

        uint16_t cur_depth     = 65535;
        bool     depth_invalid = false;
        for(int chl = 0; chl < nchannels; chl++) {
            int k = i + chl * npts;
            if(z[k] < EPSILON) {
                depth_invalid = true;
                break;
            }

            u_rgb[chl] = static_cast<int>(x[k] + 0.5), v_rgb[chl] = static_cast<int>(y[k] + 0.5);

            if(z[k] < cur_depth)
                cur_depth = static_cast<uint16_t>(z[k]);
        }

        if(depth_invalid)
            continue;

        if(map) {  // coordinates mapping for C2D
            if((u_rgb[0] >= 0) && (u_rgb[0] < rgb_intric_.width) && (v_rgb[0] >= 0) && (v_rgb[0] < rgb_intric_.height)) {
                map[2 * (point_index + i)]     = u_rgb[0];
                map[2 * (point_index + i) + 1] = v_rgb[0];
            }
        }

        if(out_depth) {
            fillPixelGap<uint16_t>(u_rgb, v_rgb, rgb_intric_.width, rgb_intric_.height, cur_depth, out_depth, gap_fill_copy_);
        }
    }
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

void AlignImpl::distortedWithSSE(__m128 &tx, __m128 &ty, const __m128 x2, const __m128 y2, const __m128 r2) {
    __m128 xy = _mm_mul_ps(tx, ty);
    __m128 r4 = _mm_mul_ps(r2, r2);
    __m128 r6 = _mm_mul_ps(r4, r2);

    // float k_jx = k1 * r2 + k2 * r4 + k3 * r6;
    __m128 k_jx = _mm_add_ps(_mm_add_ps(_mm_mul_ps(color_k1_, r2), _mm_mul_ps(color_k2_, r4)), _mm_mul_ps(color_k3_, r6));

    // float x_qx = p2 * (2 * x2 + r2) + 2 * p1 * xy;
    __m128 x_qx_1 = _mm_mul_ps(color_p2_, _mm_add_ps(_mm_mul_ps(x2, TWO), r2));
    __m128 x_qx_2 = _mm_mul_ps(_mm_mul_ps(color_p1_, xy), TWO);
    __m128 x_qx   = _mm_add_ps(x_qx_1, x_qx_2);

    // float y_qx = p1 * (2 * y2 + r2) + 2 * p2 * xy;
    __m128 y_qx_1 = _mm_mul_ps(color_p1_, _mm_add_ps(_mm_mul_ps(y2, TWO), r2));
    __m128 y_qx_2 = _mm_mul_ps(_mm_mul_ps(color_p2_, xy), TWO);
    __m128 y_qx   = _mm_add_ps(y_qx_1, y_qx_2);

    // float distx = tx * k_jx + x_qx;
    __m128 distx_lo = _mm_add_ps(_mm_mul_ps(tx, k_jx), x_qx);
    // float disty = ty * k_jx + y_qx;
    __m128 disty_lo = _mm_add_ps(_mm_mul_ps(ty, k_jx), y_qx);

    // tx = tx + distx;
    // ty = ty + disty;
    tx = _mm_add_ps(tx, distx_lo);
    ty = _mm_add_ps(ty, disty_lo);
}

void AlignImpl::D2CWithoutSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x, const float *coeff_mat_y, const float *coeff_mat_z,
                              int *map) {

    int       channel     = (gap_fill_copy_ ? 1 : 2);
    float *   ptr_coeff_x = (float *)coeff_mat_x;
    float *   ptr_coeff_y = (float *)coeff_mat_y;
    float *   ptr_coeff_z = (float *)coeff_mat_z;
    uint16_t *ptr_depth   = (uint16_t *)depth_buffer;

    for(int v = 0; v < depth_intric_.height; v += 1) {
        int depth_idx = v * depth_intric_.width;
        for(int u = 0; u < depth_intric_.width; u += 1) {
            uint16_t depth = *ptr_depth++;
            depth_idx++;
            if(depth < EPSILON) {
                ptr_coeff_x += channel;
                ptr_coeff_y += channel;
                ptr_coeff_z += channel;
                continue;
            }
            // int   u_rgb[] = { -1, -1 };
            // int   v_rgb[] = { -1, -1 };
            float pixelx_f[2], pixely_f[2], dst[2];

            bool skip_this_pixel = true;
            for(int k = 0; k < channel; k++) {
                float dst_x = depth * (*ptr_coeff_x++) + scaled_trans_[0];
                float dst_y = depth * (*ptr_coeff_y++) + scaled_trans_[1];
                dst[k]      = depth * (*ptr_coeff_z++) + scaled_trans_[2];

                float tx = float(dst_x / dst[k]);
                float ty = float(dst_y / dst[k]);

                if(add_target_distortion_) {
                    float pt_ud[2] = { tx, ty };
                    float pt_d[2]  = { 0 };
                    float r2_cur   = pt_ud[0] * pt_ud[0] + pt_ud[1] * pt_ud[1];
                    if((OB_DISTORTION_BROWN_CONRADY_K6 == rgb_disto_.model) && (r2_max_loc_ != 0) && (r2_cur > r2_max_loc_)) {
                        continue;  // break;
                    }
                    addDistortion(rgb_disto_, pt_ud, pt_d);
                    tx = pt_d[0];
                    ty = pt_d[1];
                }

                pixelx_f[k]     = tx * rgb_intric_.fx + rgb_intric_.cx;
                pixely_f[k]     = ty * rgb_intric_.fy + rgb_intric_.cy;
                skip_this_pixel = false;
            }

            if(!skip_this_pixel)
                transferDepth(pixelx_f, pixely_f, dst, 1, depth_idx - 1, out_depth, map);
        }
    }
}

void AlignImpl::D2CWithSSE(const uint16_t *depth_buffer, uint16_t *out_depth, const float *coeff_mat_x, const float *coeff_mat_y, const float *coeff_mat_z,
                           int *map) {

    int channel = (gap_fill_copy_ ? 1 : 2);
    for(int i = 0; i < depth_intric_.width * depth_intric_.height; i += 8) {
        __m128i depth_i16 = _mm_loadu_si128((__m128i *)(depth_buffer + i));
        __m128i depth_i[] = { _mm_unpacklo_epi16(depth_i16, ZERO), _mm_unpackhi_epi16(depth_i16, ZERO) };

        // transform 8 points each group
        // int group_idx = i * channel;

        // lower half or higher half 4 points of that group
        for(int k = 0; k < 2; k++) {

            // int half_idx = group_idx + k * 4 * channel;
            int half_idx = (i + k * 4) * channel;

            __m128 depth_o, nx, ny;
            __m128 depth_sse = _mm_cvtepi32_ps(depth_i[k]);
            float  x[8]      = { 0 };
            float  y[8]      = { 0 };
            float  z[8]      = { 0 };

            // center or top-left-and-bottom-right
            for(int fold = 0; fold < channel; fold++) {
                // int coeff_idx = half_idx + fold * 2 * channel;
                int    coeff_idx  = half_idx;
                float  coeff_x[4] = { coeff_mat_x[coeff_idx + fold], coeff_mat_x[coeff_idx + 1 * channel + fold], coeff_mat_x[coeff_idx + 2 * channel + fold],
                                     coeff_mat_x[coeff_idx + 3 * channel + fold] };
                float  coeff_y[4] = { coeff_mat_y[coeff_idx + fold], coeff_mat_y[coeff_idx + 1 * channel + fold], coeff_mat_y[coeff_idx + 2 * channel + fold],
                                     coeff_mat_y[coeff_idx + 3 * channel + fold] };
                float  coeff_z[4] = { coeff_mat_z[coeff_idx + fold], coeff_mat_z[coeff_idx + 1 * channel + fold], coeff_mat_z[coeff_idx + 2 * channel + fold],
                                     coeff_mat_z[coeff_idx + 3 * channel + fold] };
                __m128 coeff_sse1 = _mm_loadu_ps(coeff_x);
                __m128 coeff_sse2 = _mm_loadu_ps(coeff_y);
                __m128 coeff_sse3 = _mm_loadu_ps(coeff_z);

                __m128 X = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse1), scaled_trans_1_);
                __m128 Y = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse2), scaled_trans_2_);
                depth_o  = _mm_add_ps(_mm_mul_ps(depth_sse, coeff_sse3), scaled_trans_3_);

                nx = _mm_div_ps(X, depth_o);
                ny = _mm_div_ps(Y, depth_o);

                if(add_target_distortion_) {
                    __m128 x2 = _mm_mul_ps(nx, nx);
                    __m128 y2 = _mm_mul_ps(ny, ny);
                    __m128 r2 = _mm_add_ps(x2, y2);

                    __m128 flag = _mm_or_ps(_mm_cmpge_ps(ZERO_F, r2_max_loc_sse_), _mm_cmplt_ps(r2, r2_max_loc_sse_));
                    switch(rgb_disto_.model) {
                    case OB_DISTORTION_BROWN_CONRADY:
                        distortedWithSSE(nx, ny, x2, y2, r2);
                        break;
                    case OB_DISTORTION_BROWN_CONRADY_K6:
                        depth_o = _mm_and_ps(depth_o, flag);
                        BMDistortedWithSSE(nx, ny, x2, y2, r2);
                        break;
                    case OB_DISTORTION_KANNALA_BRANDT4:
                        KBDistortedWithSSE(nx, ny, r2);
                        break;
                    default:
                        LOG_ERROR("Distortion model not supported yet");
                        break;
                    }
                }

                __m128 pixelx = _mm_add_ps(_mm_mul_ps(nx, color_fx_), color_cx_);
                __m128 pixely = _mm_add_ps(_mm_mul_ps(ny, color_fy_), color_cy_);
                _mm_storeu_ps(x + fold * 4, pixelx);
                _mm_storeu_ps(y + fold * 4, pixely);
                _mm_storeu_ps(z + fold * 4, depth_o);
            }

            transferDepth(x, y, z, 4, i + k * 4, out_depth, map);
        }
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

    int pixnum = color_width * color_height;
    if(out_depth) {
        // init to 1s (depth 0 may be used as other useful date)
        memset(out_depth, 0xff, pixnum * sizeof(uint16_t));
    }

    const float *coeff_mat_x = finder_x->second;
    const float *coeff_mat_y = finder_y->second;
    const float *coeff_mat_z = finder_z->second;

    if(withSSE) {
        D2CWithSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
    }
    else {
        D2CWithoutSSE(depth_buffer, out_depth, coeff_mat_x, coeff_mat_y, coeff_mat_z, map);
    }

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
    int *              depth_xy = new int[size];
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
            memset(out_rgb, 0, depth_width * depth_height * sizeof(uint24_t));
            mapPixel<uint24_t>(depth_xy, static_cast<const uint24_t *>(rgb_buffer), color_width, color_height, (uint24_t *)out_rgb, depth_width, depth_height);
            break;
        case OB_FORMAT_BGR:
        case OB_FORMAT_BGRA:
        case OB_FORMAT_RGBA:
        case OB_FORMAT_MJPG:
        case OB_FORMAT_YUYV:
        default:
            LOG_ERROR("Not supported yet");
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
