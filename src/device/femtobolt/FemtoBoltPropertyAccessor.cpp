#include "FemtoBoltPropertyAccessor.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"
#include "property/InternalProperty.hpp"
#include "rawphase/RawPhaseStreamer.hpp"

namespace libobsensor {
FemtoBoltPropertyAccessor::FemtoBoltPropertyAccessor(IDevice *owner) : owner_(owner) {}


void FemtoBoltPropertyAccessor::setPropertyValue(uint32_t propertyId, OBPropertyValue value) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->setPropertyValue(propertyId, value);

    if(propertyId == OB_PROP_SWITCH_IR_MODE_INT) {
        auto rawphaseStreamer = owner_->getComponentT<RawPhaseStreamer>(OB_DEV_COMPONENT_RAW_PHASE_STREAMER);
        rawphaseStreamer->setIsPassiveIR((bool)value.intValue);
    }
}

void FemtoBoltPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyValue(propertyId, value);
}

void FemtoBoltPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyRange(propertyId, range);
}

}  // namespace libobsensor