// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIPropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

OpenNIDisp2DepthPropertyAccessor::OpenNIDisp2DepthPropertyAccessor(IDevice *owner) : owner_(owner), swDisparityToDepthEnabled_(true) {}

void OpenNIDisp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
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

        currentDepthUnitLevel_ = value.intValue;

    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void OpenNIDisp2DepthPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
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

void OpenNIDisp2DepthPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
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

void OpenNIDisp2DepthPropertyAccessor::markOutputDisparityFrame(bool enable) {
    auto sensor = owner_->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
    if(sensor) {
        sensor->markOutputDisparityFrame(enable);
    }
}

void OpenNIDisp2DepthPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(data);
    if(propertyId == OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST) {
        throw invalid_value_exception("OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST is read-only");
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

const std::vector<uint8_t> &OpenNIDisp2DepthPropertyAccessor::getStructureData(uint32_t propertyId) {
    if(propertyId == OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST) {
        static std::vector<uint16_t> swD2DSupportList = { OB_PRECISION_1MM, OB_PRECISION_0MM8, OB_PRECISION_0MM4, OB_PRECISION_0MM2, OB_PRECISION_0MM1 };
        static std::vector<uint8_t>  swD2DSupportListBytes(reinterpret_cast<uint8_t *>(swD2DSupportList.data()),
                                                          reinterpret_cast<uint8_t *>(swD2DSupportList.data()) + swD2DSupportList.size() * 2);
        return swD2DSupportListBytes;
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

}  // namespace libobsensor
