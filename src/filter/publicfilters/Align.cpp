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

Align::Align() : align_to_stream_(OB_STREAM_COLOR) {
    pImpl = new AlignImpl();
    memset(&from_intrin_, 0, sizeof(OBCameraIntrinsic));
    memset(&from_disto_, 0, sizeof(OBCameraDistortion));
    memset(&to_intrin_, 0, sizeof(OBCameraIntrinsic));
    memset(&to_disto_, 0, sizeof(OBCameraDistortion));
    memset(&from_to_extrin_, 0, sizeof(OBTransform));
    depth_unit_mm_         = 1.f;
    add_target_distortion_ = true;
    gap_fill_copy_         = false;
}

Align::~Align() noexcept {
    reset();
    if(pImpl) {
        delete pImpl;
        pImpl = nullptr;
    }
}

void Align::updateConfig(std::vector<std::string> &params) {
    // AlignType, TargetDistortion, GapFillCopy
    std::lock_guard<std::recursive_mutex> lock(alignMutex_);
    if(params.size() != 3) {
        throw invalid_value_exception("Align config error: params size not match");
    }
    try {
        int align_to_stream = std::stoi(params[0]);
        if(align_to_stream >= OB_STREAM_IR && align_to_stream <= OB_STREAM_IR_RIGHT) {
            align_to_stream_ = (OBStreamType)align_to_stream;
        }
        add_target_distortion_ = bool(std::stoi(params[1]));
        gap_fill_copy_         = bool(std::stoi(params[2]));
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("Align config error: " + std::string(e.what()));
    }
}

const std::string &Align::getConfigSchema() const {
    static const std::string schema = "AlignType, integer, 1, 7, 1, 2, aligned to the type of data stream\n"
                                      "TargetDistortion, boolean, 0, 1, 1, 1, add distortion of the target stream\n"
                                      "GapFillCopy, boolean, 0, 1, 1, 0, enable gap fill";
    return schema;
}

void Align::reset() {
    pImpl->reset();
}

std::shared_ptr<Frame> Align::process(std::shared_ptr<const Frame> frame) {
    std::lock_guard<std::recursive_mutex> lock(alignMutex_);
    if(!frame->is<FrameSet>()) {
        LOG_WARN("Invalid frame!");
        return FrameFactory::createFrameFromOtherFrame(frame, true);
    }

    auto newFrame = FrameFactory::createFrameFromOtherFrame(frame);
    auto frames   = newFrame->as<FrameSet>();
    // get type of align_to_stream_
    auto existFrame = frames->getFrame(getAlignFrameType());
    if(!existFrame) {
        return frames;
    }

    auto depth = frames->getFrame(OB_FRAME_DEPTH);
    if(!depth) {
        LOG_WARN("Invalid depth frame!");
        return frames;
    }

    auto depthFormat = depth->getFormat();
    if(!(depthFormat == OB_FORMAT_Z16 || depthFormat == OB_FORMAT_Y16)) {
        LOG_WARN("Invalid depth frame!");
        return frames;
    }

    depth_unit_mm_ = depth->as<DepthFrame>()->getValueScale();

    // prepare "other" data buffer to vector of Frame
    std::vector<std::shared_ptr<const Frame>> other_frames;
    if(align_to_stream_ == OB_STREAM_DEPTH) {  // other to depth
        uint32_t frameCount = frames->getCount();
        for(uint32_t i = 0; i < frameCount; i++) {
            std::shared_ptr<const Frame> item = frames->getFrame(i);
            if((item->getType() != OB_FRAME_DEPTH) && item->is<VideoFrame>()) {
                other_frames.push_back(item);
            }
        }
    }
    else {
        uint32_t frameCount = frames->getCount();
        for(uint32_t i = 0; i < frameCount; i++) {
            std::shared_ptr<const Frame> item = frames->getFrame(i);
            if((item->getType() == streamTypeToFrameType.at(align_to_stream_))) {
                other_frames.push_back(item);
            }
        }
    }

    // create aligned buffer and do alignment
    std::shared_ptr<Frame> aligned_frame = nullptr;
    if(align_to_stream_ == OB_STREAM_DEPTH) {  // other to depth
        for(auto from: other_frames) {
            if(!from) {
                LOG_ERROR("from is nullptr!");
                continue;
            }
            auto original_profile = from->getStreamProfile()->as<VideoStreamProfile>();
            auto to_profile       = depth->getStreamProfile()->as<VideoStreamProfile>();
            auto alignProfile     = createAlignedProfile(original_profile, to_profile);

            aligned_frame = FrameFactory::createVideoFrame(from->getType(), from->getFormat(), alignProfile->getWidth(), alignProfile->getHeight(), 0);
            if(aligned_frame) {
                aligned_frame->copyInfoFromOther(from);
                aligned_frame->setStreamProfile(alignProfile);
                alignFrames(aligned_frame, from, depth);
                frames->pushFrame(std::move(aligned_frame));
            }
        }
    }
    else {
        auto to = other_frames.front();  // depth to other

        auto original_profile  = depth->getStreamProfile()->as<VideoStreamProfile>();
        auto to_profile        = to->getStreamProfile()->as<VideoStreamProfile>();
        auto alignProfile      = createAlignedProfile(original_profile, to_profile);

        aligned_frame = FrameFactory::createVideoFrame(depth->getType(), depth->getFormat(), alignProfile->getWidth(), alignProfile->getHeight(), 0);
        if(aligned_frame) {
            aligned_frame->copyInfoFromOther(depth);
            aligned_frame->setStreamProfile(alignProfile);
            alignFrames(aligned_frame, depth, to);
            frames->pushFrame(std::move(aligned_frame));
        }
    }

    return frames;
}

OBFrameType Align::getAlignFrameType() {
    switch(align_to_stream_) {
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

void Align::alignFrames(std::shared_ptr<Frame> aligned, const std::shared_ptr<const Frame> from, const std::shared_ptr<const Frame> to) {
    if((!from) || (!to) || (!aligned)) {
        LOG_ERROR_INTVL("Null pointer is unexpected");
        return;
    }
    auto fromProfile = from->getStreamProfile();
    auto toProfile   = to->getStreamProfile();

    auto fromVideoProfile = fromProfile->as<VideoStreamProfile>();
    from_intrin_          = fromVideoProfile->getIntrinsic();
    from_disto_           = fromVideoProfile->getDistortion();
    auto toVideoProfile   = toProfile->as<VideoStreamProfile>();
    to_intrin_            = toVideoProfile->getIntrinsic();
    to_disto_             = toVideoProfile->getDistortion();
    from_to_extrin_       = fromProfile->getExtrinsicTo(toProfile);

    if(to->getType() == OB_FRAME_DEPTH) {
        uint8_t *alignedData = reinterpret_cast<uint8_t *>(const_cast<void *>((void *)aligned->getData()));
        memset(alignedData, 0, aligned->getDataSize());
        // check if already initialized inside
        auto depth_other_extrin = toProfile->getExtrinsicTo(fromProfile);
        pImpl->initialize(to_intrin_, to_disto_, from_intrin_, from_disto_, depth_other_extrin, depth_unit_mm_, add_target_distortion_, gap_fill_copy_);
        auto depth = reinterpret_cast<const uint16_t *>(to->getData());
        auto in    = const_cast<const void *>((const void *)from->getData());
        auto out   = const_cast<void *>((void *)aligned->getData());
        pImpl->C2D(depth, toVideoProfile->getWidth(), toVideoProfile->getHeight(), in, out, fromVideoProfile->getWidth(), fromVideoProfile->getHeight(),
                   from->getFormat());
    }
    else {
        uint16_t *alignedData = reinterpret_cast<uint16_t *>(const_cast<void *>((void *)aligned->getData()));
        memset(alignedData, 0, aligned->getDataSize());
        pImpl->initialize(from_intrin_, from_disto_, to_intrin_, to_disto_, from_to_extrin_, depth_unit_mm_, add_target_distortion_, gap_fill_copy_);
        auto in = reinterpret_cast<const uint16_t *>(from->getData());
        pImpl->D2C(in, fromVideoProfile->getWidth(), fromVideoProfile->getHeight(), alignedData, toVideoProfile->getWidth(), toVideoProfile->getHeight());
    }
}

std::shared_ptr<VideoStreamProfile> Align::createAlignedProfile(std::shared_ptr<const VideoStreamProfile> original_profile,
                                                                std::shared_ptr<const VideoStreamProfile> to_profile) {

    if(align_streams_.first != original_profile.get() || align_streams_.second != to_profile.get()) {
        auto aligned_profile = original_profile->clone()->as<VideoStreamProfile>();
        aligned_profile->setWidth(to_profile->getWidth());
        aligned_profile->setHeight(to_profile->getHeight());
        aligned_profile->bindIntrinsic(to_profile->getIntrinsic());
        aligned_profile->bindDistortion(to_profile->getDistortion());
        aligned_profile->bindSameExtrinsicTo(to_profile);
        target_stream_profile_ = aligned_profile;
        align_streams_         = { original_profile.get(), to_profile.get() };
    }

    return target_stream_profile_;
}

}  // namespace libobsensor

