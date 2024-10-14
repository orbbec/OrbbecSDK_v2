// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PrivateFilterPropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"

namespace libobsensor {
PrivateFilterPropertyAccessor::PrivateFilterPropertyAccessor(IDevice *device) : device_(device) {}

void PrivateFilterPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    auto processor = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    processor->setPropertyValue(propertyId, value);
}

void PrivateFilterPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto processor = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    processor->getPropertyValue(propertyId, value);
}

void PrivateFilterPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto processor = device_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
    processor->getPropertyRange(propertyId, range);
}
}  // namespace libobsensor
