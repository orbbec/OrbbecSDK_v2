// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrameMetadataModifier.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class DabaiALGMSLMetadataModifier : public IFrameMetadataModifier, public DeviceComponentBase {
public:
    DabaiALGMSLMetadataModifier(IDevice *owner);
    virtual ~DabaiALGMSLMetadataModifier() override;

    void modify(std::shared_ptr<Frame> frame) override;
};

}  // namespace libobsensor