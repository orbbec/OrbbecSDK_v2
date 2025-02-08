// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "frameprocessor/FrameProcessor.hpp"

namespace libobsensor {

class G435LeDepthFrameProcessor : public DepthFrameProcessor {
public:
    G435LeDepthFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~G435LeDepthFrameProcessor() noexcept;
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;
};

class G435LeColorFrameProcessor : public FrameProcessor {
public:
    G435LeColorFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context);
    virtual ~G435LeColorFrameProcessor() noexcept;
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;
};

}  // namespace libobsensor
