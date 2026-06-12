// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HardwareD2CPropertyAccessor.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

HardwareD2CPropertyAccessor::HardwareD2CPropertyAccessor(IDevice *owner, std::shared_ptr<IBasicPropertyAccessor> delegate)
    : owner_(owner), delegate_(std::move(delegate)) {}

void HardwareD2CPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    if(propertyId == OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL && owner_->hasAnyVideoSensorStreamActivated()) {
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("Hardware D2C cannot be toggled while any video sensor stream is active.");
    }
    delegate_->setPropertyValue(propertyId, value);
}

void HardwareD2CPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    delegate_->getPropertyValue(propertyId, value);
}

void HardwareD2CPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    delegate_->getPropertyRange(propertyId, range);
}

}  // namespace libobsensor
