// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "IStreamer.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

/**
 * @brief ILiDARStreamer interface
 */
class ILiDARStreamer : public IStreamer, public IDeviceComponent {
public:
    virtual ~ILiDARStreamer() = default;

    // IStreamer
    virtual void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) = 0;
    virtual void stopStream(std::shared_ptr<const StreamProfile> profile)                                 = 0;
    // IDeviceComponent
    virtual IDevice *getOwner() const = 0;
};

}  // namespace libobsensor
