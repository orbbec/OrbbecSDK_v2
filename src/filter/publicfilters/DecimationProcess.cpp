// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DecimationProcess.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

#define SWAP(a, b)            \
    {                         \
        uint16_t temp = (a);  \
        (a)           = (b);  \
        (b)           = temp; \
    }

inline uint16_t median1(uint16_t arr[]) {
    return arr[0];
}

inline uint16_t median2(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    return arr[0];
}

inline uint16_t median3(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[1] > arr[2])
        SWAP(arr[1], arr[2]);
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    return arr[1];
}

inline uint16_t median4(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[2] > arr[3])
        SWAP(arr[2], arr[3]);
    if(arr[0] > arr[2])
        SWAP(arr[0], arr[2]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    if(arr[1] > arr[2])
        SWAP(arr[1], arr[2]);
    return arr[1];
}

inline uint16_t median5(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    if(arr[0] > arr[2])
        SWAP(arr[0], arr[2]);
    if(arr[1] > arr[2])
        SWAP(arr[1], arr[2]);
    if(arr[3] > arr[2])
        SWAP(arr[3], arr[2]);
    if(arr[4] > arr[2])
        SWAP(arr[4], arr[2]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    return arr[2];
}

inline uint16_t median6(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[2] > arr[3])
        SWAP(arr[2], arr[3]);
    if(arr[4] > arr[5])
        SWAP(arr[4], arr[5]);
    if(arr[0] > arr[2])
        SWAP(arr[0], arr[2]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    if(arr[2] > arr[4])
        SWAP(arr[2], arr[4]);
    if(arr[3] > arr[5])
        SWAP(arr[3], arr[5]);
    if(arr[1] > arr[4])
        SWAP(arr[1], arr[4]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    return arr[2];
}

inline uint16_t median7(uint16_t arr[]) {
    if(arr[0] > arr[5])
        SWAP(arr[0], arr[5]);
    if(arr[0] > arr[3])
        SWAP(arr[0], arr[3]);
    if(arr[1] > arr[6])
        SWAP(arr[1], arr[6]);
    if(arr[2] > arr[4])
        SWAP(arr[2], arr[4]);
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[3] > arr[5])
        SWAP(arr[3], arr[5]);
    if(arr[2] > arr[6])
        SWAP(arr[2], arr[6]);
    if(arr[2] > arr[3])
        SWAP(arr[2], arr[3]);
    if(arr[3] > arr[6])
        SWAP(arr[3], arr[6]);
    if(arr[4] > arr[5])
        SWAP(arr[4], arr[5]);
    if(arr[1] > arr[5])
        SWAP(arr[1], arr[5]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    return arr[3];
}

inline uint16_t median8(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[2] > arr[3])
        SWAP(arr[2], arr[3]);
    if(arr[4] > arr[5])
        SWAP(arr[4], arr[5]);
    if(arr[6] > arr[7])
        SWAP(arr[6], arr[7]);
    if(arr[0] > arr[2])
        SWAP(arr[0], arr[2]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    if(arr[4] > arr[6])
        SWAP(arr[4], arr[6]);
    if(arr[5] > arr[7])
        SWAP(arr[5], arr[7]);
    if(arr[1] > arr[4])
        SWAP(arr[1], arr[4]);
    if(arr[3] > arr[6])
        SWAP(arr[3], arr[6]);
    if(arr[2] > arr[5])
        SWAP(arr[2], arr[5]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    if(arr[2] > arr[6])
        SWAP(arr[2], arr[6]);
    if(arr[1] > arr[3])
        SWAP(arr[1], arr[3]);
    if(arr[5] > arr[7])
        SWAP(arr[5], arr[7]);
    if(arr[3] > arr[5])
        SWAP(arr[3], arr[5]);
    if(arr[4] > arr[6])
        SWAP(arr[4], arr[6]);
    return arr[3];
}

inline uint16_t median9(uint16_t arr[]) {
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    if(arr[6] > arr[7])
        SWAP(arr[6], arr[7]);
    if(arr[1] > arr[2])
        SWAP(arr[1], arr[2]);
    if(arr[4] > arr[5])
        SWAP(arr[4], arr[5]);
    if(arr[7] > arr[8])
        SWAP(arr[7], arr[8]);
    if(arr[0] > arr[1])
        SWAP(arr[0], arr[1]);
    if(arr[3] > arr[4])
        SWAP(arr[3], arr[4]);
    if(arr[6] > arr[7])
        SWAP(arr[6], arr[7]);
    if(arr[1] > arr[2])
        SWAP(arr[1], arr[2]);
    if(arr[4] > arr[5])
        SWAP(arr[4], arr[5]);
    if(arr[7] > arr[8])
        SWAP(arr[7], arr[8]);
    arr[3] = arr[0] > arr[3] ? arr[0] : arr[3];
    arr[5] = arr[5] > arr[8] ? arr[8] : arr[5];
    if(arr[4] > arr[7])
        SWAP(arr[4], arr[7]);
    arr[6] = arr[3] > arr[6] ? arr[3] : arr[6];
    arr[4] = arr[1] > arr[4] ? arr[1] : arr[4];
    arr[2] = arr[2] > arr[5] ? arr[5] : arr[2];
    arr[4] = arr[4] > arr[7] ? arr[7] : arr[4];
    if(arr[4] > arr[2])
        SWAP(arr[4], arr[2]);
    arr[4] = arr[4] > arr[6] ? arr[6] : arr[4];
    return arr[4];
}

typedef uint16_t (*MDFUNC)(uint16_t arr[]);
// 0     1       2        3        4         5        6        7       8       9
static MDFUNC _mdfunc[] = { 0, median1, median2, median3, median4, median5, median6, median7, median8, median9 };

DecimationFilter::DecimationFilter()
    : decimation_factor_(2),
      control_val_(2),
      patch_size_(2),
      kernel_size_(patch_size_ * patch_size_),
      real_width_(0),
      real_height_(0),
      padded_width_(0),
      padded_height_(0),
      recalc_profile_(false),
      options_changed_(false) {}

DecimationFilter::~DecimationFilter() noexcept {}

void DecimationFilter::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 1) {
        throw invalid_value_exception("DecimationFilter config error: params size not match");
    }
    try {
        uint8_t value = static_cast<uint8_t>(std::stoul(params[0]));
        if(value >= 1 && value <= 8) {
            if(value != control_val_) {
                control_val_       = value;
                patch_size_        = control_val_;
                decimation_factor_ = control_val_;
                kernel_size_       = patch_size_ * patch_size_;
                options_changed_   = true;
            }
        }
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("DecimationFilter config error: " + std::string(e.what()));
    }
}

const std::string &DecimationFilter::getConfigSchema() const {
    // csv format: name，type， min，max，step，default，description
    static const std::string schema = "decimate, int, 1, 8, 1, 2, value decimate factor";
    return schema;
}

void DecimationFilter::reset() {
    options_changed_ = true;
    registered_profiles_.clear();
    source_stream_profile_.reset();
    target_stream_profile_.reset();
}

std::shared_ptr<Frame> DecimationFilter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    if(frame->is<FrameSet>()) {
        LOG_WARN_INTVL("The Frame processed by DecimationFilter cannot be FrameSet!");
        auto outFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
        return outFrame;
    }

    if(!isFrameFormatTypeSupported(frame->getFormat())) {
        LOG_WARN_INTVL("Unsupported decimation filter processing frame format @{}.", frame->getFormat());
        auto outFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
        return outFrame;
    }

    updateOutputProfile(frame);

    OBFormat frameFormat = frame->getFormat();
    auto     newOutFrame = FrameFactory::createFrameFromStreamProfile(target_stream_profile_);

    newOutFrame->copyInfoFromOther(frame);
    auto srcVideosFrame = frame->as<VideoFrame>();
    auto newVideoFrame  = newOutFrame->as<VideoFrame>();
    if(frame->getType() == OB_FRAME_DEPTH && (frameFormat == OB_FORMAT_Y16 || frameFormat == OB_FORMAT_Z16)) {
        decimateDepth((uint16_t *)frame->getData(), (uint16_t *)newVideoFrame->getData(), srcVideosFrame->getWidth(), patch_size_);
    }
    else {
        decimateOthers(frameFormat, (void *)frame->getData(), (void *)newVideoFrame->getData(), srcVideosFrame->getWidth(), patch_size_);
    }

    return newOutFrame;
}

bool DecimationFilter::isFrameFormatTypeSupported(OBFormat type) {
    switch(type) {
    case OB_FORMAT_Y16:
    case OB_FORMAT_Z16:
    case OB_FORMAT_Y8:
    case OB_FORMAT_YUYV:
    case OB_FORMAT_UYVY:
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        return true;
    default:
        break;
    }

    return false;
}

void DecimationFilter::updateOutputProfile(const std::shared_ptr<const Frame> frame) {
    auto streamProfile = frame->getStreamProfile();
    if(options_changed_ || streamProfile.get() != source_stream_profile_.get()) {
        options_changed_       = false;
        source_stream_profile_ = streamProfile->as<VideoStreamProfile>();
        const auto pf          = registered_profiles_.find(std::make_tuple(source_stream_profile_.get(), decimation_factor_));
        if(registered_profiles_.end() != pf) {
            target_stream_profile_ = pf->second;

            // Update real/padded output frame size based on retrieved input properties
            real_width_    = (uint16_t)source_stream_profile_->getWidth() / patch_size_;
            real_height_   = (uint16_t)source_stream_profile_->getHeight() / patch_size_;
            padded_width_  = (uint16_t)target_stream_profile_->getWidth();
            padded_height_ = (uint16_t)target_stream_profile_->getHeight();
        }
        else {
            recalc_profile_ = true;
        }
    }

    // Build a new target profile for every system/filter change
    if(recalc_profile_) {
        auto source_vsp = source_stream_profile_->as<VideoStreamProfile>();

        // recalculate real/padded output frame size based on new input porperties
        real_width_  = (uint16_t)source_vsp->getWidth() / patch_size_;
        real_height_ = (uint16_t)source_vsp->getHeight() / patch_size_;

        // The resulted frame dimension will be dividable by 4;
        padded_width_ = real_width_ + 3;
        padded_width_ /= 4;
        padded_width_ *= 4;

        padded_height_ = real_height_ + 3;
        padded_height_ /= 4;
        padded_height_ *= 4;

        auto intrinsic   = source_vsp->getIntrinsic();
        intrinsic.width  = padded_width_;
        intrinsic.height = padded_height_;
        intrinsic.fx     = intrinsic.fx / patch_size_;
        intrinsic.fy     = intrinsic.fy / patch_size_;
        intrinsic.cx     = intrinsic.cx / patch_size_;
        intrinsic.cy     = intrinsic.cy / patch_size_;

        target_stream_profile_ = source_vsp->clone()->as<VideoStreamProfile>();
        target_stream_profile_->setWidth(padded_width_);
        target_stream_profile_->setHeight(padded_height_);
        // extrinsic and distortion parameters remain unchanged.
        target_stream_profile_->bindIntrinsic(intrinsic);

        registered_profiles_[std::make_tuple(source_stream_profile_.get(), decimation_factor_)] = target_stream_profile_;

        recalc_profile_ = false;
    }
}

void DecimationFilter::decimateDepth(uint16_t *frame_data_in, uint16_t *frame_data_out, size_t width_in, size_t scale) {

    // construct internal register buf
    uint16_t  working_kernel[9];
    uint16_t *pixel_raws[10];  // max scale set 10
    uint16_t *block_start = const_cast<uint16_t *>(frame_data_in);
    uint16_t *p{};
    int       wk_count = 0;
    int       wk_sum   = 0;
    MDFUNC    f;

    if(scale == 2 || scale == 3) {
        // loop through rows
        for(int j = 0; j < real_height_; j++) {

            for(size_t i = 0; i < scale; i++) {
                pixel_raws[i] = block_start + (width_in * i);
                //__builtin_prefetch(pixel_raws[i] + (width_in * scale), 0, 3);
            }

            // processing row-wisely
            for(size_t i = 0, chunk_offset = 0; i < real_width_; i++, chunk_offset += scale) {
                wk_count = 0;
                // processing kernel
                for(size_t n = 0; n < scale; ++n) {
                    p = pixel_raws[n] + chunk_offset;
                    for(size_t m = 0; m < scale; ++m) {
                        if(*(p + m)) {
                            working_kernel[wk_count] = *(p + m);
                            wk_count++;
                        }
                    }
                }

                if(wk_count == 0)
                    *frame_data_out++ = 0;
                else {
                    f                 = _mdfunc[wk_count];
                    *frame_data_out++ = f(working_kernel);
                }
            }

            // filling right side blanks
            for(int k = real_width_; k < padded_width_; k++)
                *frame_data_out++ = 0;

            // move to the start position of next processing block
            block_start += width_in * scale;
        }
    }
    else {

        for(int j = 0; j < real_height_; j++) {

            for(size_t i = 0; i < scale; i++) {
                pixel_raws[i] = block_start + (width_in * i);
            }

            for(size_t i = 0, chunk_offset = 0; i < real_width_; i++, chunk_offset += scale) {
                wk_sum   = 0;
                wk_count = 0;
                for(size_t n = 0; n < scale; ++n) {
                    p = pixel_raws[n] + chunk_offset;
                    for(size_t m = 0; m < scale; ++m) {
                        if(*(p + m)) {
                            wk_sum += p[m];
                            ++wk_count;
                        }
                    }
                }

                *frame_data_out++ = (uint16_t)(wk_count == 0 ? 0 : wk_sum / wk_count);
            }

            for(int k = real_width_; k < padded_width_; k++)
                *frame_data_out++ = 0;

            block_start += width_in * scale;
        }
    }
    memset(frame_data_out, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint16_t));
}

void DecimationFilter::decimateOthers(OBFormat format, void *frame_data_in, void *frame_data_out, size_t width_in, size_t scale) {

    auto patch_size = scale * scale;

    int wk_sum = 0;
    // int wk_count = 0;

    switch(format) {
    case OB_FORMAT_YUYV: {
        uint8_t *from = (uint8_t *)frame_data_in;
        uint8_t *p    = nullptr;
        uint8_t *q    = (uint8_t *)frame_data_out;

        auto w_2  = width_in >> 1;
        auto rw_2 = real_width_ >> 1;
        auto pw_2 = padded_width_ >> 1;
        auto s2   = scale >> 1;
        bool odd  = (scale & 1);
        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < rw_2; ++i) {
                p      = from + scale * (j * w_2 + i) * 4;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m)
                        wk_sum += p[m * 2];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + 1;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < s2; ++m)
                        wk_sum += 2 * p[m * 4];

                    if(odd)
                        wk_sum += p[s2 * 4];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + s2 * 4 + (odd ? 2 : 0);
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m)
                        wk_sum += p[m * 2];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + 3;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < s2; ++m)
                        wk_sum += 2 * p[m * 4];

                    if(odd)
                        wk_sum += p[s2 * 4];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);
            }

            for(int i = rw_2; i < pw_2; ++i) {
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
            }
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint8_t) * 2);
    } break;

    case OB_FORMAT_UYVY: {
        uint8_t *from = (uint8_t *)frame_data_in;
        uint8_t *p    = nullptr;
        uint8_t *q    = (uint8_t *)frame_data_out;

        auto w_2  = width_in >> 1;
        auto rw_2 = real_width_ >> 1;
        auto pw_2 = padded_width_ >> 1;
        auto s2   = scale >> 1;
        bool odd  = (scale & 1);
        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < rw_2; ++i) {
                p      = from + scale * (j * w_2 + i) * 4;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < s2; ++m)
                        wk_sum += 2 * p[m * 4];

                    if(odd)
                        wk_sum += p[s2 * 4];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + 1;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m)
                        wk_sum += p[m * 2];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + 2;
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < s2; ++m)
                        wk_sum += 2 * p[m * 4];

                    if(odd)
                        wk_sum += p[s2 * 4];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);

                p      = from + scale * (j * w_2 + i) * 4 + s2 * 4 + (odd ? 3 : 1);
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m)
                        wk_sum += p[m * 2];

                    p += w_2 * 4;
                }
                *q++ = (uint8_t)(wk_sum / patch_size);
            }

            for(int i = rw_2; i < pw_2; ++i) {
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
            }
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint8_t) * 2);
    } break;

    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR: {
        uint8_t *from = (uint8_t *)frame_data_in;
        uint8_t *p    = nullptr;
        uint8_t *q    = (uint8_t *)frame_data_out;
        ;

        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < real_width_; ++i) {
                for(int k = 0; k < 3; ++k) {
                    p      = from + scale * (j * width_in + i) * 3 + k;
                    wk_sum = 0;
                    for(size_t n = 0; n < scale; ++n) {
                        for(size_t m = 0; m < scale; ++m)
                            wk_sum += p[m * 3];

                        p += width_in * 3;
                    }

                    *q++ = (uint8_t)(wk_sum / patch_size);
                }
            }

            for(int i = real_width_; i < padded_width_; ++i) {
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
            }
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint8_t) * 3);
    } break;

    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA: {
        uint8_t *from = (uint8_t *)frame_data_in;
        uint8_t *p    = nullptr;
        uint8_t *q    = (uint8_t *)frame_data_out;

        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < real_width_; ++i) {
                for(int k = 0; k < 4; ++k) {
                    p      = from + scale * (j * width_in + i) * 4 + k;
                    wk_sum = 0;
                    for(size_t n = 0; n < scale; ++n) {
                        for(size_t m = 0; m < scale; ++m)
                            wk_sum += p[m * 4];

                        p += width_in * 4;
                    }

                    *q++ = (uint8_t)(wk_sum / patch_size);
                }
            }

            for(int i = real_width_; i < padded_width_; ++i) {
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
                *q++ = 0;
            }
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint8_t) * 4);

    } break;

    case OB_FORMAT_Y8: {
        uint8_t *from = (uint8_t *)frame_data_in;
        uint8_t *p    = nullptr;
        uint8_t *q    = (uint8_t *)frame_data_out;

        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < real_width_; ++i) {
                p      = from + scale * (j * width_in + i);
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m) {
                        wk_sum += p[m];
                    }

                    p += width_in;
                }

                *q++ = (uint8_t)(wk_sum / patch_size);
            }

            for(int i = real_width_; i < padded_width_; ++i)
                *q++ = 0;
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint8_t));

    } break;

    case OB_FORMAT_Y16: {
        uint16_t *from = (uint16_t *)frame_data_in;
        uint16_t *p    = nullptr;
        uint16_t *q    = (uint16_t *)frame_data_out;

        for(int j = 0; j < real_height_; ++j) {
            for(int i = 0; i < real_width_; ++i) {
                p      = from + scale * (j * width_in + i);
                wk_sum = 0;
                for(size_t n = 0; n < scale; ++n) {
                    for(size_t m = 0; m < scale; ++m) {
                        wk_sum += p[m];
                    }
                    p += width_in;
                }

                *q++ = (uint16_t)(wk_sum / patch_size);
            }

            for(int i = real_width_; i < padded_width_; ++i)
                *q++ = 0;
        }

        memset(q, 0, (padded_height_ - real_height_) * padded_width_ * sizeof(uint16_t));

    } break;

    default:
        break;
    }
}

}  // namespace libobsensor

