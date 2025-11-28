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
    ~ILiDARStreamer() override = default;

    // IStreamer
    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override = 0;
    void stopStream(std::shared_ptr<const StreamProfile> profile) override                                 = 0;
    // IDeviceComponent
    IDevice *getOwner() const override = 0;
};

}  // namespace libobsensor
