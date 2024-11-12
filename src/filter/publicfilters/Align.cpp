// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Align.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "frame/FrameMemoryPool.hpp"

namespace libobsensor {

const std::map<OBStreamType, OBFrameType> streamTypeToFrameType = { { OB_STREAM_COLOR, OB_FRAME_COLOR },
                                                                    { OB_STREAM_DEPTH, OB_FRAME_DEPTH },
                                                                    { OB_STREAM_IR, OB_FRAME_IR },
                                                                    { OB_STREAM_IR_LEFT, OB_FRAME_IR_LEFT },
                                                                    { OB_STREAM_IR_RIGHT, OB_FRAME_IR_RIGHT } };

Align::Align()
    : impl_(new AlignImpl()),  //
      alignToStreamType_(OB_STREAM_COLOR),
      addTargetDistortion_(true),
      gapFillCopy_(true),
      matchTargetRes_(true) {}

Align::~Align() noexcept {
    reset();
    delete impl_;
}

void Align::updateConfig(std::vector<std::string> &params) {
    // AlignType, TargetDistortion, GapFillCopy, matchTargetRes
    std::lock_guard<std::recursive_mutex> lock(alignMutex_);
    if(params.size() != 4) {
        throw invalid_value_exception("Align config error: params size not match");
    }
    try {
        int align_to_stream = std::stoi(params[0]);
        if(align_to_stream >= OB_STREAM_IR && align_to_stream <= OB_STREAM_IR_RIGHT) {
            alignToStreamType_ = (OBStreamType)align_to_stream;
        }
        addTargetDistortion_ = bool(std::stoi(params[1]));
        gapFillCopy_         = bool(std::stoi(params[2]));
        matchTargetRes_      = bool(std::stoi(params[3]));
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("Align config error: " + std::string(e.what()));
    }
}

const std::string &Align::getConfigSchema() const {
    // csv format: name, type, min, max, step, default, description
    static const std::string schema = "AlignType, integer, 1, 7, 1, 2, align to the type of data stream\n"
                                      "TargetDistortion, boolean, 0, 1, 1, 1, add distortion of the target stream\n"
                                      "GapFillCopy, boolean, 0, 1, 1, 0, enable gap fill\n"
                                      "MatchTargetRes, boolean, 0, 1, 1, 1, enable match the output resolution to the align target resolution\n";
    return schema;
}

void Align::reset() {
    impl_->reset();
}

std::shared_ptr<Frame> Align::process(std::shared_ptr<const Frame> frame) {
    std::lock_guard<std::recursive_mutex> lock(alignMutex_);

    std::shared_ptr<const DepthFrame> depth;
    if(frame->is<FrameSet>()) {
        auto fset = frame->as<FrameSet>();
        auto df   = fset->getFrame(OB_FRAME_DEPTH);
        if(!df) {
            LOG_WARN("Invalid input frame, not depth frame found!");
            return nullptr;
        }
        depth = df->as<DepthFrame>();
    }
    else if(frame->is<DepthFrame>()) {
        depth = frame->as<DepthFrame>();
    }
    else {
        LOG_WARN("Invalid input frame, not depth frame found!");
        return nullptr;
    }

    auto depthFormat = depth->getFormat();
    if(!(depthFormat == OB_FORMAT_Z16 || depthFormat == OB_FORMAT_Y16)) {
        LOG_WARN("Invalid depth frame  format{}, only support Z16 or Y16!", depthFormat);
        return nullptr;
    }

    // prepare "other" frames for alignment if
    std::vector<std::shared_ptr<const Frame>> other_frames;
    if(frame->is<FrameSet>()) {
        auto fset = frame->as<FrameSet>();
        if(alignToStreamType_ == OB_STREAM_DEPTH) {  // other to depth
            uint32_t frameCount = fset->getCount();
            for(uint32_t i = 0; i < frameCount; i++) {
                std::shared_ptr<const Frame> item = fset->getFrame(i);
                if((item->getType() != OB_FRAME_DEPTH) && item->is<VideoFrame>()) {
                    other_frames.push_back(item);
                }
            }
        }
        else {
            uint32_t frameCount = fset->getCount();
            for(uint32_t i = 0; i < frameCount; i++) {
                std::shared_ptr<const Frame> item = fset->getFrame(i);
                if((item->getType() == streamTypeToFrameType.at(alignToStreamType_))) {
                    other_frames.push_back(item);
                }
            }
        }
    }

    std::shared_ptr<Frame> outFrame;
    // create align buffer and do alignment
    if(alignToStreamType_ == OB_STREAM_DEPTH) {  // other to depth
        if(!frame->is<FrameSet>()) {
            LOG_WARN("Invalid input frame, other frame align to depth only support FrameSet!");
        }
        outFrame = FrameFactory::createFrameFromOtherFrame(frame);
        auto to  = depth;
        for(auto from: other_frames) {
            auto fromProfile    = from->getStreamProfile()->as<VideoStreamProfile>();
            auto toProfile      = to->getStreamProfile()->as<VideoStreamProfile>();
            auto alignToProfile = createAlignedProfile(fromProfile, toProfile);
            auto alignToFrame   = FrameFactory::createFrameFromStreamProfile(alignToProfile);
            alignToFrame->copyInfoFromOther(from);
            alignFrames(from, alignToFrame, depth);

            auto outFrameSet = outFrame->as<FrameSet>();
            outFrameSet->pushFrame(std::move(alignToFrame));
        }
    }
    else {
        auto from = depth;
        if(other_frames.empty() && !alignProfile_ && !toProfile_) {
            LOG_WARN("No other frame or align profile or to profile found!");
            return nullptr;
        }

        std::shared_ptr<VideoStreamProfile> alignToProfile;
        if(!other_frames.empty()) {
            auto fromProfile = depth->getStreamProfile()->as<VideoStreamProfile>();
            auto to          = other_frames.front();  // depth to other
            auto toProfile   = to->getStreamProfile()->as<VideoStreamProfile>();
            alignToProfile   = createAlignedProfile(fromProfile, toProfile);
        }
        else if(toProfile_) {
            auto fromProfile = depth->getStreamProfile()->as<VideoStreamProfile>();
            alignToProfile   = createAlignedProfile(fromProfile, toProfile_);
        }
        else {
            alignToProfile = alignProfile_;
        }
        auto alignToFrame = FrameFactory::createFrameFromStreamProfile(alignToProfile);
        alignToFrame->copyInfoFromOther(depth);
        alignFrames(from, alignToFrame, depth);
        if(frame->is<FrameSet>() && !outFrame) {
            outFrame         = FrameFactory::createFrameFromOtherFrame(frame);
            auto outFrameSet = outFrame->as<FrameSet>();
            outFrameSet->pushFrame(std::move(alignToFrame));
        }
        else {
            outFrame = alignToFrame;
        }
    }

    return outFrame;
}

OBFrameType Align::getAlignFrameType() {
    switch(alignToStreamType_) {
    case OB_STREAM_DEPTH:
        return OB_FRAME_DEPTH;
    case OB_STREAM_COLOR:
        return OB_FRAME_COLOR;
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

void Align::alignFrames(const std::shared_ptr<const Frame> from, std::shared_ptr<Frame> align, const std::shared_ptr<const Frame> depth) {
    if((!from) || (!align)) {
        LOG_ERROR_INTVL("Null pointer is unexpected");
        return;
    }
    auto fromProfile  = from->getStreamProfile();
    auto alignProfile = align->getStreamProfile();

    auto fromVideoProfile  = fromProfile->as<VideoStreamProfile>();
    auto fromIntrin        = fromVideoProfile->getIntrinsic();
    auto fromDisto         = fromVideoProfile->getDistortion();
    auto alignVideoProfile = alignProfile->as<VideoStreamProfile>();
    auto toIntrin          = alignVideoProfile->getIntrinsic();
    auto toDisto           = alignVideoProfile->getDistortion();
    auto fromToExtrin      = fromProfile->getExtrinsicTo(alignProfile);
    auto depthUnitMm       = depth->as<DepthFrame>()->getValueScale();

    if(align->getType() == OB_FRAME_DEPTH) {
        uint16_t *alignedData = reinterpret_cast<uint16_t *>(const_cast<void *>((void *)align->getData()));
        impl_->initialize(fromIntrin, fromDisto, toIntrin, toDisto, fromToExtrin, depthUnitMm, addTargetDistortion_, gapFillCopy_);
        auto in = reinterpret_cast<const uint16_t *>(from->getData());
        impl_->D2C(in, fromVideoProfile->getWidth(), fromVideoProfile->getHeight(), alignedData, alignVideoProfile->getWidth(), alignVideoProfile->getHeight());
    }
    else {
        if(!depth) {
            LOG_ERROR_INTVL("depth is nullptr!");
            return;
        }
        auto depth_other_extrin = alignProfile->getExtrinsicTo(fromProfile);
        impl_->initialize(toIntrin, toDisto, fromIntrin, fromDisto, depth_other_extrin, depthUnitMm, addTargetDistortion_, gapFillCopy_);
        auto dep = reinterpret_cast<const uint16_t *>(depth->getData());
        auto in  = const_cast<const void *>((const void *)from->getData());
        auto out = const_cast<void *>((void *)align->getData());
        impl_->C2D(dep, alignVideoProfile->getWidth(), alignVideoProfile->getHeight(), in, out, fromVideoProfile->getWidth(), fromVideoProfile->getHeight(),
                   from->getFormat());
    }
}

void estimateFOV(OBCameraIntrinsic intrin, float *fovs) {
    fovs[0] = atan2f(intrin.cx, intrin.fx) + atan2f(intrin.width - intrin.cx - 1.f, intrin.fx);
    fovs[1] = atan2f(intrin.cy, intrin.fy) + atan2f(intrin.height - intrin.cy - 1.f, intrin.fy);
}

std::shared_ptr<VideoStreamProfile> Align::createAlignedProfile(std::shared_ptr<const VideoStreamProfile> fromProfile,
                                                                std::shared_ptr<const VideoStreamProfile> toProfile) {
    if(fromProfile_ != fromProfile || toProfile_ != toProfile) {
        auto alignProfile = fromProfile->clone()->as<VideoStreamProfile>();
        alignProfile->setWidth(toProfile->getWidth());
        alignProfile->setHeight(toProfile->getHeight());
        alignProfile->bindIntrinsic(toProfile->getIntrinsic());
        alignProfile->bindDistortion(toProfile->getDistortion());
        alignProfile->bindSameExtrinsicTo(toProfile);

        if(!matchTargetRes_ && alignProfile->getType() == OB_STREAM_DEPTH) {
            // Not match to target resolution required, scale to match the source resolution but keep the aspect ratio same as the target.

            OBCameraIntrinsic intrin  = alignProfile->getIntrinsic();
            float             scale_x = 1.f * toProfile->getWidth() / fromProfile->getWidth();
            float             scale_y = 1.f * toProfile->getHeight() / fromProfile->getHeight();
            float             scale   = scale_x > scale_y ? scale_x : scale_y;
            if(scale > 1.499) {
                float s = 1.f * int(scale) + 0.5f * (int(scale + 0.5) - int(scale));
                intrin.fx /= s;
                intrin.fy /= s;
                intrin.cx /= s;
                intrin.cy /= s;
                intrin.width  = static_cast<int16_t>(0.5f + (intrin.width >> 1) / s) << 1;
                intrin.height = static_cast<int16_t>(0.5f + (intrin.height >> 1) / s) << 1;
            }
            else if(1.0f / scale > 1.499) {
                float s = 1.f / (1.f * int(1.f / scale) + 0.5f * (int(1.f / scale + 0.5) - int(1.f / scale)));
                intrin.fx /= s;
                intrin.fy /= s;
                intrin.cx /= s;
                intrin.cy /= s;
                intrin.width  = static_cast<int16_t>(0.5f + (intrin.width >> 1) / s) << 1;
                intrin.height = static_cast<int16_t>(0.5f + (intrin.height >> 1) / s) << 1;
            }

            alignProfile->setWidth(intrin.width);
            alignProfile->setHeight(intrin.height);
            alignProfile->bindIntrinsic(intrin);
        }

        fromProfile_  = fromProfile;
        toProfile_    = toProfile;
        alignProfile_ = alignProfile;

        float ori_fov[2], to_fov[2];
        estimateFOV(fromProfile->getIntrinsic(), ori_fov);
        estimateFOV(toProfile->getIntrinsic(), to_fov);

        float ori_pix_per_ang[2] = { fromProfile->getWidth() / ori_fov[0], fromProfile->getHeight() / ori_fov[1] },
              to_pix_per_ang[2]  = { toProfile->getWidth() / to_fov[0], toProfile->getHeight() / to_fov[1] };

        if(alignToStreamType_ == OBStreamType::OB_STREAM_DEPTH) {
            gapFillCopy_ = true;
        }
        else {
            if((ori_pix_per_ang[0] / to_pix_per_ang[0] > 0.5f) && (ori_pix_per_ang[1] / to_pix_per_ang[1] > 0.5f)) {
                gapFillCopy_ = true;
            }
            else {
                gapFillCopy_ = false;
            }
        }

    }

    return alignProfile_;
}

}  // namespace libobsensor
