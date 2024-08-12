#include "HdrMergeProcess.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "libobsensor/h/ObTypes.h"
#include "utils/PublicTypeHelper.hpp"

namespace libobsensor {

// avoids returning too old merged frame - frame counter jumps forward
const int SEQUENTIAL_FRAMES_THRESHOLD = 4;
std::pair<OBFormat, std::vector<uint8_t>> EXP_LUT_;

template <typename T> void generateConfidenceMap(const T *ir, uint8_t *map, int width, int height) {
    const T *p_ir  = ir;
    uint8_t *p_map = map;
    for(size_t i = 0; i < height; i++) {
        for(size_t j = 0; j < width; j++) {
            *p_map++ = EXP_LUT_[*p_ir++];
        }
    }
}

template <typename T> void triangleWeights(std::vector<uint8_t> &w) {
    int length = 1 << (sizeof(T) * 8);
    int half   = length >> 2;
    for(int i = 0; i < length; i++) {
        uint8_t tmp = static_cast<uint8_t>(255.f * i / half);
        if(i > half)
            tmp = static_cast<uint8_t>(512 - 255.f * i / half);
        w.push_back(tmp);
    }
}

template <typename T> void mergeFramesUsingIr(uint16_t *new_data, uint16_t *d0, uint16_t *d1, const T *ir0, const T *ir1, int width, int height) {
    int pix_num = width * height;

    for(int i = 0; i < pix_num; i++) {
        uint8_t c0  = EXP_LUT_.second[ir0[i]];
        uint8_t c1  = EXP_LUT_.second[ir1[i]];
        new_data[i] = c0 > c1 ? d0[i] : d1[i];
    }
}

void mergeFramesUsingOnlyDepth(uint16_t *new_data, uint16_t *d0, uint16_t *d1, int width, int height) {
    for(int i = 0; i < width * height; i++) {
        if(d0[i] && d1[i]) {
            if(d0[i] == 65535)
                new_data[i] = d1[i];
            else
                new_data[i] = d0[i];
        }
        else if(d0[i])
            new_data[i] = d0[i];
        else if(d1[i])
            new_data[i] = d1[i];
    }
}

std::shared_ptr<const IRFrame> getIRFrameFromFrameSet(std::shared_ptr<const Frame> frame_fs) {
    if(!frame_fs->is<FrameSet>()) {
        return nullptr;
    }

    auto frameSet = frame_fs->as<FrameSet>();
    auto irFrame  = frameSet->getFrame(OB_FRAME_IR);
    if(!irFrame) {
        irFrame = frameSet->getFrame(OB_FRAME_IR_LEFT);
        if(!irFrame) {
            irFrame = frameSet->getFrame(OB_FRAME_IR_RIGHT);
        }
    }

    if(irFrame) {
        return irFrame->as<IRFrame>();
    }

    return nullptr;
}

bool checkIRAvailability(std::shared_ptr<const DepthFrame> first_depth, std::shared_ptr<const IRFrame> first_ir, std::shared_ptr<const DepthFrame> second_depth,
                         std::shared_ptr<const IRFrame> second_ir) {

    bool use_ir = (first_ir && second_ir);

    if(use_ir) {
        if((first_depth->getWidth() != first_ir->getWidth()) || (first_depth->getHeight() != first_ir->getHeight())
           || (second_ir->getWidth() != first_ir->getWidth()) || (second_ir->getHeight() != first_ir->getHeight()))
            use_ir = false;

        if(use_ir) {
            auto depth_info = first_depth->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
            auto ir_info    = first_ir->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
            use_ir          = (depth_info == ir_info);

            if(use_ir) {
                depth_info = second_depth->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
                ir_info    = second_ir->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
                use_ir     = (depth_info == ir_info);

                if(use_ir) {
                    depth_info = first_depth->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX);
                    ir_info    = first_ir->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX);
                    use_ir     = (depth_info == ir_info);

                    if(use_ir) {
                        depth_info = second_depth->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX);
                        ir_info    = second_ir->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX);
                        use_ir     = (depth_info == ir_info);

                        if(use_ir) {
                            OBFormat format = first_ir->getFormat();
                            use_ir          = (second_ir->getFormat() == format);

                            if(use_ir) {
                                // other format not supported yet
                                use_ir = ((OB_FORMAT_Y8 == format) || (OB_FORMAT_Y16 == format));
                            }
                        }
                    }
                }
            }
        }
    }

    return use_ir;
}

HDRMerge::HDRMerge(const std::string &name) : FilterBase(name) {}
HDRMerge::~HDRMerge() noexcept {}

void HDRMerge::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("HDRMerge update config error: unsupported operation.");
    }
}

const std::string &HDRMerge::getConfigSchema() const {
    static const std::string schema = "";
    return schema;
}

std::shared_ptr<Frame> HDRMerge::processFunc(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    std::shared_ptr<const DepthFrame> depthFrame = nullptr;
    if(frame->is<FrameSet>()) {
        depthFrame = frame->as<FrameSet>()->getFrame(OB_FRAME_DEPTH)->as<DepthFrame>();
    }
    else {
        depthFrame = frame->as<DepthFrame>();
    }

    if(!depthFrame) {
        LOG_WARN_INTVL("No depth frame found, hdrMerge unsupported to process this frame");
        std::shared_ptr<Frame> outFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
        return outFrame;
    }

    auto depthSeqSize = depthFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_SIZE);
    if(depthSeqSize != 2) {
        LOG_WARN_INTVL("HDRMerge unsupported to process this frame with sequence size: {}", depthSeqSize);
        std::shared_ptr<Frame> outFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
        return outFrame;
    }

    auto depth_seq_id = depthFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX);
    int64_t frameSize = static_cast<int64_t>(frames_.size());
    if(frameSize == depth_seq_id) {
        frames_[depth_seq_id] = depthFrame;
    }

    discardDepthMergedFrameIfNeeded(depthFrame);

    if(frames_.size() >= 2) {
        auto frame_0             = frames_[0];
        auto frame_0_framenumber = frame_0->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
        auto frame_1             = frames_[1];
        auto frame_1_framenumber = frame_1->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
        frames_.clear();

        // two adjancent frames
        if(1 == frame_1_framenumber - frame_0_framenumber) {
            std::shared_ptr<Frame> new_frame = merge(frame_0, frame_1);
            if(new_frame) {
                depth_merged_frame_ = new_frame;
            }
        }
    }

    if(depth_merged_frame_) {
        if(frame->is<FrameSet>()) {
            auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);
            auto frameSet = outFrame->as<FrameSet>();
            frameSet->pushFrame(std::move(depth_merged_frame_));
            return outFrame;
        }
        else {
            return depth_merged_frame_;
        }
    }

    std::shared_ptr<Frame> outFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
    return outFrame;
}

void HDRMerge::discardDepthMergedFrameIfNeeded(std::shared_ptr<const Frame> frame) {
    if(depth_merged_frame_) {
        std::shared_ptr<const DepthFrame> newFrame = nullptr;
        if(frame->is<FrameSet>()) {
            newFrame = frame->as<FrameSet>()->getFrame(OB_FRAME_DEPTH)->as<DepthFrame>();
        }
        else {
            newFrame = frame->as<DepthFrame>();
        }

        auto mergedFrame = depth_merged_frame_->as<DepthFrame>();

        auto merged_counter = depth_merged_frame_->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
        auto new_counter    = newFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
        auto counter_diff   = new_counter - merged_counter;

        // counter rest or, previous frame too old or, resolution changed
        if((counter_diff < 0) || (counter_diff > 4) || (mergedFrame->getWidth() != newFrame->getWidth())
           || (mergedFrame->getHeight() != newFrame->getHeight())) {
            depth_merged_frame_ = nullptr;
        }
    }
}

std::shared_ptr<Frame> HDRMerge::merge(std::shared_ptr<const Frame> first_fs, std::shared_ptr<const Frame> second_fs) {
    std::shared_ptr<const DepthFrame> first_depth  = nullptr;
    std::shared_ptr<const DepthFrame> second_depth = nullptr;
    std::shared_ptr<const IRFrame>    first_ir     = nullptr;
    std::shared_ptr<const IRFrame>    second_ir    = nullptr;

    if(first_fs->is<FrameSet>()) {
        first_depth = first_fs->as<FrameSet>()->getFrame(OB_FRAME_DEPTH)->as<DepthFrame>();
    }
    else {
        first_depth = first_fs->as<DepthFrame>();
    }
    first_ir = getIRFrameFromFrameSet(first_fs);

    if(second_fs->is<FrameSet>()) {
        second_depth = second_fs->as<FrameSet>()->getFrame(OB_FRAME_DEPTH)->as<DepthFrame>();
    }
    else {
        second_depth = second_fs->as<DepthFrame>();
    }
    second_ir = getIRFrameFromFrameSet(second_fs);

    auto width  = first_depth->getWidth();
    auto height = first_depth->getHeight();
    if((width != second_depth->getWidth()) || (height != second_depth->getHeight())) {
        return nullptr;
    }

    auto newFrame = FrameFactory::createFrameFromStreamProfile(first_depth->getStreamProfile());
    if(newFrame) {
        newFrame->copyInfoFromOther(first_depth);
        auto d0 = (uint16_t *)first_depth->getData();
        auto d1 = (uint16_t *)second_depth->getData();
        auto d  = (uint16_t *)newFrame->getData();
        memset(d, 0, newFrame->getDataSize());
        if(checkIRAvailability(first_depth, first_ir, second_depth, second_ir)) {
            OBFormat ir_format = first_ir->getFormat();
            if((EXP_LUT_.first != ir_format) || (EXP_LUT_.second.empty())) {
                EXP_LUT_.first = ir_format;
                if(ir_format == OB_FORMAT_Y8)
                    triangleWeights<uint8_t>(EXP_LUT_.second);
                else
                    triangleWeights<uint16_t>(EXP_LUT_.second);
            }
            if(OB_FORMAT_Y8 == ir_format) {
                mergeFramesUsingIr<uint8_t>(d, d0, d1, first_ir->getData(), second_ir->getData(), width, height);
            }
            else {
                mergeFramesUsingIr<uint16_t>(d, d0, d1, (uint16_t *)first_ir->getData(), (uint16_t *)second_ir->getData(), width, height);
            }
        }
        else {
            mergeFramesUsingOnlyDepth(d, d0, d1, width, height);
        }
        return newFrame;
    }

    return FrameFactory::createFrameFromOtherFrame(first_fs, true);
}

}  // namespace libobsensor
