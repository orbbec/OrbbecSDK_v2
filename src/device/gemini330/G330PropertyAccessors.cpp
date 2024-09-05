#include "G330PropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

G330Disp2DepthPropertyAccessor::G330Disp2DepthPropertyAccessor(IDevice *owner)
    : owner_(owner), hwDisparityToDepthEnabled_(true), swDisparityToDepthEnabled_(false) {}

void G330Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);

        // swDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);
        // update convert output frame as disparity frame
        // markOutputDisparityFrame(!hwDisparityToDepthEnabled_);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
        hwDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);

        // update convert output frame as disparity frame
        markOutputDisparityFrame(!hwDisparityToDepthEnabled_);
    } break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);

        // update depth unit
        auto sensor          = owner_->getComponentT<ISensor>(OB_DEV_COMPONENT_DEPTH_SENSOR).get();
        auto disparitySensor = std::dynamic_pointer_cast<DisparityBasedSensor>(sensor);
        if(disparitySensor) {
            disparitySensor->setDepthUnit(value.floatValue);
        }

    } break;

    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void G330Disp2DepthPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
        swDisparityToDepthEnabled_ = static_cast<bool>(value->intValue);
    } break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT: {
        auto            commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        OBPropertyValue hwDisparityValue;
        commandPort->getPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, &hwDisparityValue);
        if(hwDisparityValue.intValue == 0) {
            auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
            processor->getPropertyValue(propertyId, value);
        }
        else {
            commandPort->getPropertyValue(propertyId, value);
        }
    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(propertyId, value);
    } break;
    }
}

void G330Disp2DepthPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT: {
        auto            processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        OBPropertyValue swDisparityEnable;
        processor->getPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, &swDisparityEnable);
        if(swDisparityEnable.intValue == 1) {
            processor->getPropertyRange(propertyId, range);
        }
        else {
            auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->getPropertyRange(propertyId, range);
        }
    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyRange(propertyId, range);
    } break;
    }
}

void G330Disp2DepthPropertyAccessor::markOutputDisparityFrame(bool enable) {
    if(!owner_->isComponentExists(OB_DEV_COMPONENT_DEPTH_SENSOR)) {
        return;
    }

    auto sensor          = owner_->getComponentT<ISensor>(OB_DEV_COMPONENT_DEPTH_SENSOR).get();
    auto disparitySensor = std::dynamic_pointer_cast<DisparityBasedSensor>(sensor);
    if(disparitySensor) {
        disparitySensor->markOutputDisparityFrame(enable);
    }
}

}  // namespace libobsensor