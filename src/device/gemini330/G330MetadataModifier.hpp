// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrameMetadataModifier.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class G330GMSLMetadataModifier : public IFrameMetadataModifier, public DeviceComponentBase {
public:
    G330GMSLMetadataModifier(IDevice *owner);
    virtual ~G330GMSLMetadataModifier() override;

    void modify(std::shared_ptr<Frame> frame) override;
};

}  // namespace libobsensor