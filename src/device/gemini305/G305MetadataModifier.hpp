// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrameMetadataModifier.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class G305GMSLMetadataModifier : public IFrameMetadataModifier, public DeviceComponentBase {
public:
    G305GMSLMetadataModifier(IDevice *owner);
    virtual ~G305GMSLMetadataModifier() override;

    void modify(std::shared_ptr<Frame> frame) override;
};

}  // namespace libobsensor