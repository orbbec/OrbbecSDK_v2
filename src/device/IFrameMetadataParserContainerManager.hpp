// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDeviceComponent.hpp"
#include "IFrame.hpp"

namespace libobsensor {
class IFrameMetadataParserContainerManager : virtual public IDeviceComponent, virtual public IFrameMetadataParserContainer {};
}  // namespace libobsensor
