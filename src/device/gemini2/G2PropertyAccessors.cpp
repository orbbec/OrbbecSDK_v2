// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G2PropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

G2Disp2DepthPropertyAccessor::G2Disp2DepthPropertyAccessor(IDevice *owner) : owner_(owner), hwDisparityToDepthEnabled_(true) {}

IDevice *G2Disp2DepthPropertyAccessor::getOwner() {
    return owner_;
}

void G2Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        if(value.intValue == 1) {
            OBPropertyValue precisionLevel;
            precisionLevel.intValue = hwD2DSupportList_.front();
            setPropertyValue(OB_PROP_DEPTH_PRECISION_LEVEL_INT, precisionLevel);
        }
        auto propertyAccessor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyAccessor->setPropertyValue(propertyId, value);
        hwDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);
        markOutputDisparityFrame(!hwDisparityToDepthEnabled_);
    } break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto            processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        OBPropertyValue swDisparityEnable;
        processor->getPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, &swDisparityEnable);
        if(swDisparityEnable.intValue == 1) {
            processor->setPropertyValue(propertyId, value);
        }
        else {
            auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->setPropertyValue(propertyId, value);
        }

        // update depth unit
        auto sensor = owner_->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
        if(sensor) {
            auto depthUnit = utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue));
            sensor->setDepthUnit(depthUnit);
        }
    } break;

    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void G2Disp2DepthPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        value->intValue = hwDisparityToDepthEnabled_;
    } break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto            processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        OBPropertyValue swDisparityEnable;
        processor->getPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, &swDisparityEnable);
        if(swDisparityEnable.intValue == 1) {
            processor->getPropertyValue(propertyId, value);
        }
        else {
            auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->getPropertyValue(propertyId, value);
        }
    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(propertyId, value);
    } break;
    }
}

void G2Disp2DepthPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        range->min.intValue  = 0;
        range->max.intValue  = 1;
        range->step.intValue = 1;
        range->def.intValue  = 1;
        range->cur.intValue  = static_cast<int32_t>(hwDisparityToDepthEnabled_);
    } break;
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
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

void G2Disp2DepthPropertyAccessor::markOutputDisparityFrame(bool enable) {
    auto sensor = owner_->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
    if(sensor) {
        sensor->markOutputDisparityFrame(enable);
    }
}

void G2Disp2DepthPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(data);
    if(propertyId == OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST) {
        throw invalid_value_exception("OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST is read-only");
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

const std::vector<uint8_t> &G2Disp2DepthPropertyAccessor::getStructureData(uint32_t propertyId) {
    if(propertyId == OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST) {
        if(hwDisparityToDepthEnabled_) {

            static std::vector<uint8_t> hwD2DSupportListBytes(reinterpret_cast<const uint8_t *>(hwD2DSupportList_.data()),
                                                              reinterpret_cast<const uint8_t *>(hwD2DSupportList_.data()) + hwD2DSupportList_.size() * 2);
            return hwD2DSupportListBytes;
        }
        else {
            static std::vector<uint8_t> swD2DSupportListBytes(reinterpret_cast<const uint8_t *>(swD2DSupportList_.data()),
                                                              reinterpret_cast<const uint8_t *>(swD2DSupportList_.data()) + swD2DSupportList_.size() * 2);
            return swD2DSupportListBytes;
        }
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

G210Disp2DepthPropertyAccessor::G210Disp2DepthPropertyAccessor(IDevice *owner) : G2Disp2DepthPropertyAccessor(owner) {
    hwD2DSupportList_ = { OB_PRECISION_0MM4, OB_PRECISION_0MM1, OB_PRECISION_0MM05 };
}

void G210Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto            owner     = getOwner();
        auto            processor = owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        OBPropertyValue swDisparityEnable;
        processor->getPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, &swDisparityEnable);
        if(swDisparityEnable.intValue == 1) {
            processor->setPropertyValue(propertyId, value);
        }
        else {
            auto            commandPort             = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            auto            realPrecisionLevelValue = utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue)) / 0.5f;
            OBPropertyValue propertyValue;
            propertyValue.intValue = static_cast<int32_t>(utils::depthUnitToPrecisionLevel(realPrecisionLevelValue));
            commandPort->setPropertyValue(propertyId, propertyValue);
        }

        // update depth unit
        auto sensor = owner->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
        if(sensor) {
            auto depthUnit = utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value.intValue));
            sensor->setDepthUnit(depthUnit);
        }
    } break;
    default: {
        G2Disp2DepthPropertyAccessor::setPropertyValue(propertyId, value);
    }
    }
}

void G210Disp2DepthPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_DEPTH_PRECISION_LEVEL_INT: {
        auto            owner     = getOwner();
        auto            processor = owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        OBPropertyValue swDisparityEnable;
        processor->getPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, &swDisparityEnable);
        if(swDisparityEnable.intValue == 1) {
            processor->getPropertyValue(propertyId, value);
        }
        else {
            auto commandPort = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->getPropertyValue(propertyId, value);
            auto realPrecisionLevelValue = utils::depthPrecisionLevelToUnit(static_cast<OBDepthPrecisionLevel>(value->intValue)) * 0.5f;
            value->intValue              = static_cast<int32_t>(utils::depthUnitToPrecisionLevel(realPrecisionLevelValue));
        }
    } break;
    default: {
        G2Disp2DepthPropertyAccessor::getPropertyValue(propertyId, value);
    } break;
    }
}

}  // namespace libobsensor
