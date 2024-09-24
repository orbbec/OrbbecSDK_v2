#pragma once
#include "IDeviceComponent.hpp"
#include "IFrame.hpp"

namespace libobsensor {
class IFrameMetadataParserContainerManager : virtual public IDeviceComponent, virtual public IFrameMetadataParserContainer {};
}  // namespace libobsensor