// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IStreamProfile.hpp"

namespace libobsensor {

class IStreamer {
public:
    virtual ~IStreamer() noexcept = default;
    virtual void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) = 0;
    virtual void stopStream(std::shared_ptr<const StreamProfile> profile)                                 = 0;
};
}  // namespace libobsensor