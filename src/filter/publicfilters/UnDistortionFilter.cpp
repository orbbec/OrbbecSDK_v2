// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UnDistortionFilter.hpp"
#include "UnDistortionImplGeneric.hpp"
#if defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__) || (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#include "UnDistortionImplSSE.hpp"
#endif
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "utils/Utils.hpp"
#include <cstring>

namespace libobsensor {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static OBFrameType streamTypeToFrameType(OBStreamType st) {
    switch(st) {
    case OB_STREAM_COLOR:
        return OB_FRAME_COLOR;
    case OB_STREAM_COLOR_LEFT:
        return OB_FRAME_COLOR_LEFT;
    case OB_STREAM_COLOR_RIGHT:
        return OB_FRAME_COLOR_RIGHT;
    case OB_STREAM_DEPTH:
        return OB_FRAME_DEPTH;
    case OB_STREAM_IR:
        return OB_FRAME_IR;
    case OB_STREAM_IR_LEFT:
        return OB_FRAME_IR_LEFT;
    case OB_STREAM_IR_RIGHT:
        return OB_FRAME_IR_RIGHT;
    default:
        return OB_FRAME_UNKNOWN;
    }
}

static bool isDistortionTrivial(const OBCameraDistortion &d) {
    if(d.model == OB_DISTORTION_NONE) {
        return true;
    }
    return d.k1 == 0.0f && d.k2 == 0.0f && d.k3 == 0.0f && d.k4 == 0.0f && d.k5 == 0.0f && d.k6 == 0.0f && d.p1 == 0.0f && d.p2 == 0.0f;
}

// ---------------------------------------------------------------------------

UnDistortionFilter::UnDistortionFilter()
    :
#if defined(__ARM_NEON__) || defined(__NEON__) || defined(__SSSE3__) || (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
      impl_(std::make_shared<UnDistortionImplSSE>()),
#else
      impl_(std::make_shared<UnDistortionImplGeneric>()),
#endif
      streamType_(OB_STREAM_COLOR),
      newCameraIntrinsic_{},
      interpMode_(UNDIST_INTERP_BILINEAR),
      mjpgOutputFormat_(0),
      initialized_(false),
      cachedIntrinsic_{},
      cachedDistortion_{},
      cachedFormat_(OB_FORMAT_UNKNOWN) {
}

UnDistortionFilter::~UnDistortionFilter() noexcept {
    reset();
}

void UnDistortionFilter::updateConfig(std::vector<std::string> &params) {
    // Config order must match getConfigSchema():
    // [0] StreamType  [1] NewCameraFx  [2] NewCameraFy  [3] NewCameraCx  [4] NewCameraCy
    // [5] NewCameraWidth  [6] NewCameraHeight  [7] InterpolationMode  [8] MjpgOutputFormat
    if(params.size() != 9) {
        THROW_INVALID_PARAM_EXCEPTION("UnDistortionFilter config error: expected 9 params, got " + std::to_string(params.size()));
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    try {
        streamType_                = static_cast<OBStreamType>(std::stoi(params[0]));
        newCameraIntrinsic_.fx     = std::stof(params[1]);
        newCameraIntrinsic_.fy     = std::stof(params[2]);
        newCameraIntrinsic_.cx     = std::stof(params[3]);
        newCameraIntrinsic_.cy     = std::stof(params[4]);
        newCameraIntrinsic_.width  = static_cast<int16_t>(std::stoi(params[5]));
        newCameraIntrinsic_.height = static_cast<int16_t>(std::stoi(params[6]));
        interpMode_                = std::stoi(params[7]);
        int newMjpgFmt             = std::stoi(params[8]);
        if(newMjpgFmt != mjpgOutputFormat_) {
            mjpgOutputFormat_ = newMjpgFmt;
            mjpgConverter_.reset();  // force re-init with new target format
        }
    }
    catch(const std::exception &e) {
        THROW_INVALID_PARAM_EXCEPTION(std::string("UnDistortionFilter config parse error: ") + e.what());
    }
    initialized_ = false;
}

void UnDistortionFilter::setConfigData(void *data, uint32_t size) {
    utils::unusedVar(data);
    utils::unusedVar(size);
}

const std::string &UnDistortionFilter::getConfigSchema() const {
    // NewCameraWidth > 0 enables new-camera-matrix projection (hw_d2c use-case).
    // The filter scales NewCameraFx/Fy/Cx/Cy from the reference resolution (NewCameraWidth x NewCameraHeight)
    // to match the actual input frame resolution at process time.
    // csv format: name, type, min, max, step, default, description
    static const std::string schema =
        "StreamType, integer, 1, 12, 1, 2, stream type to undistort (OBStreamType: 1=IR 2=COLOR 3=DEPTH 6=IR_LEFT 7=IR_RIGHT 11=COLOR_LEFT 12=COLOR_RIGHT)\n"
        "NewCameraFx, float, 0, 10000, 1, 0, new camera matrix fx at reference resolution (0=pure undistortion)\n"
        "NewCameraFy, float, 0, 10000, 1, 0, new camera matrix fy at reference resolution\n"
        "NewCameraCx, float, 0, 10000, 1, 0, new camera matrix cx at reference resolution\n"
        "NewCameraCy, float, 0, 10000, 1, 0, new camera matrix cy at reference resolution\n"
        "NewCameraWidth, integer, 0, 10000, 1, 0, reference resolution width (0=pure undistortion)\n"
        "NewCameraHeight, integer, 0, 10000, 1, 0, reference resolution height\n"
        "InterpolationMode, integer, 0, 1, 1, 1, interpolation mode: 0=nearest neighbor 1=bilinear\n"
        "MjpgOutputFormat, integer, 0, 1, 1, 0, output format when input is MJPG: 0=RGB 1=BGR\n";
    return schema;
}

void UnDistortionFilter::reset() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    impl_->reset();
    if(mjpgConverter_) {
        mjpgConverter_->reset();
        mjpgConverter_.reset();
    }
    initialized_ = false;
}

// ---------------------------------------------------------------------------

bool UnDistortionFilter::isCacheValid(const OBCameraIntrinsic &intrin, const OBCameraDistortion &disto, OBFormat fmt) const {
    if(!initialized_ || fmt != cachedFormat_) {
        return false;
    }
    return std::memcmp(&intrin, &cachedIntrinsic_, sizeof(OBCameraIntrinsic)) == 0 && std::memcmp(&disto, &cachedDistortion_, sizeof(OBCameraDistortion)) == 0;
}

std::shared_ptr<Frame> UnDistortionFilter::undistortFrame(std::shared_ptr<const Frame> frame) {
    if(!frame || !frame->is<VideoFrame>()) {
        return nullptr;
    }

    auto videoFrame = frame->as<VideoFrame>();
    auto profile    = frame->getStreamProfile();
    if(!profile) {
        return nullptr;
    }
    auto videoProfile = profile->as<VideoStreamProfile>();
    if(!videoProfile) {
        return nullptr;
    }

    auto fmt = frame->getFormat();
    int  w   = static_cast<int>(videoFrame->getWidth());
    int  h   = static_cast<int>(videoFrame->getHeight());

    // Early-return: if there is no actual work to do (no distortion and no
    // new-camera-matrix projection), allocate a fresh frame with a cloned
    // profile and copy the data straight through.  This skips the expensive
    // MJPG decode and the full LUT/remap path while keeping a clean owning
    // output frame (independent profile, no aliasing with the input).
    const bool newCameraMode = (newCameraIntrinsic_.width > 0);
    if(!newCameraMode && isDistortionTrivial(videoProfile->getDistortion())) {
        auto outProfile = videoProfile->clone()->as<VideoStreamProfile>();
        auto outFrame   = FrameFactory::createFrameFromStreamProfile(outProfile);
        outFrame->copyInfoFromOther(frame);
        outFrame->updateData(frame->getData(), frame->getDataSize());
        return outFrame;
    }

    // MJPG is compressed and cannot be remapped directly; decode to RGB/BGR first.
    // The decoded frame carries the same intrinsic/distortion as the original.
    std::shared_ptr<const Frame> workFrame = frame;
    if(fmt == OB_FORMAT_MJPG) {
        OBFormat targetFmt = (mjpgOutputFormat_ == 1) ? OB_FORMAT_BGR : OB_FORMAT_RGB;
        if(!mjpgConverter_) {
            mjpgConverter_ = std::make_shared<FormatConverter>();
            mjpgConverter_->setConversion(OB_FORMAT_MJPG, targetFmt);
        }
        auto decoded = mjpgConverter_->process(frame);
        if(!decoded) {
            LOG_ERROR_INTVL("UnDistortionFilter: MJPG->RGB/BGR decode failed");
            return nullptr;
        }
        workFrame = decoded;
        fmt       = targetFmt;
    }

    auto workVideoProfile = workFrame->getStreamProfile()->as<VideoStreamProfile>();

    const auto intrin = workVideoProfile->getIntrinsic();
    const auto disto  = workVideoProfile->getDistortion();

    // New-camera-matrix mode: scale the stored (depth-calibrated) intrinsic to match the current color frame size.
    // Use separate horizontal/vertical scale factors to handle aspect-ratio differences
    // (e.g. depth 640x480 = 4:3, color 1920x1080 = 16:9).
    OBCameraIntrinsic        scaledNewCamera{};
    const OBCameraIntrinsic *newCameraPtr = nullptr;
    if(newCameraMode) {
        float scale_x   = static_cast<float>(w) / static_cast<float>(newCameraIntrinsic_.width);
        float scale_y   = static_cast<float>(h) / static_cast<float>(newCameraIntrinsic_.height);
        scaledNewCamera = newCameraIntrinsic_;
        scaledNewCamera.fx *= scale_x;
        scaledNewCamera.fy *= scale_y;
        scaledNewCamera.cx *= scale_x;
        scaledNewCamera.cy *= scale_y;
        scaledNewCamera.width  = static_cast<int16_t>(w);
        scaledNewCamera.height = static_cast<int16_t>(h);
        newCameraPtr           = &scaledNewCamera;
    }

    // Build or rebuild LUT when camera params change.
    // Special case in new-camera-matrix mode: the HW D2C pipeline may zero out the distortion
    // field of the color frame profile after warmup (to signal "pre-corrected"), while the
    // actual pixel data and source intrinsics are unchanged.  In that situation, keep the
    // existing LUT rather than rebuilding it as an identity mapping.
    const bool onlyDistortionWentTrivial = newCameraMode && initialized_ && isDistortionTrivial(disto) && !isDistortionTrivial(cachedDistortion_)
                                           && fmt == cachedFormat_ && std::memcmp(&intrin, &cachedIntrinsic_, sizeof(OBCameraIntrinsic)) == 0;

    if(!isCacheValid(intrin, disto, fmt) && !onlyDistortionWentTrivial) {
        impl_->initialize(intrin, disto, newCameraPtr, fmt, interpMode_);
        cachedIntrinsic_  = intrin;
        cachedDistortion_ = disto;
        cachedFormat_     = fmt;
        initialized_      = true;
    }

    // Build output stream profile: decoded format, zeroed distortion, updated intrinsic if new-camera-matrix mode is active
    auto outProfile = workVideoProfile->clone()->as<VideoStreamProfile>();
    if(newCameraPtr) {
        outProfile->bindIntrinsic(*newCameraPtr);
    }
    OBCameraDistortion zeroDisto{};
    zeroDisto.model = disto.model;
    outProfile->bindDistortion(zeroDisto);

    // Create output frame with the updated profile
    auto outFrame = FrameFactory::createFrameFromStreamProfile(outProfile);
    if(!outFrame) {
        LOG_ERROR_INTVL("UnDistortionFilter: failed to create output frame");
        return nullptr;
    }
    outFrame->copyInfoFromOther(workFrame);

    const uint8_t *srcData = workFrame->getData();
    uint8_t       *dstData = const_cast<uint8_t *>(outFrame->getData());

    impl_->undistort(srcData, dstData, w, h, fmt);
    return outFrame;
}

std::shared_ptr<Frame> UnDistortionFilter::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    OBFrameType targetFrameType = streamTypeToFrameType(streamType_);

    if(frame->is<FrameSet>()) {
        auto fset        = frame->as<FrameSet>();
        auto targetFrame = fset->getFrame(targetFrameType);
        if(!targetFrame) {
            // No matching stream in this FrameSet - pass through unchanged
            return FrameFactory::createFrameFromOtherFrame(frame);
        }

        auto undistorted = undistortFrame(targetFrame);
        if(!undistorted) {
            return FrameFactory::createFrameFromOtherFrame(frame);
        }

        auto outFrame    = FrameFactory::createFrameFromOtherFrame(frame);
        auto outFrameSet = outFrame->as<FrameSet>();
        outFrameSet->pushFrame(std::move(undistorted));
        return outFrame;
    }

    // Single-frame input: must match the configured stream type
    if(frame->getType() == targetFrameType) {
        return undistortFrame(frame);
    }

    // Unrecognised frame type - pass through
    return FrameFactory::createFrameFromOtherFrame(frame, true);
}

}  // namespace libobsensor
