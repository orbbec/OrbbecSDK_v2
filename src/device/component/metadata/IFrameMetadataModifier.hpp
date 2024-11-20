// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <IFrame.hpp>

namespace libobsensor {
class IFrameMetadataModifier {
public:
    virtual ~IFrameMetadataModifier() = default;

    virtual void modify(std::shared_ptr<Frame> frame) = 0;
};
}  // namespace libobsensor
