#include "Astra2PropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

Astra2Disp2DepthPropertyAccessor::Astra2Disp2DepthPropertyAccessor(IDevice *owner)
    : owner_(owner), hwDisparityToDepthEnabled_(true), swDisparityToDepthEnabled_(false) {}

void Astra2Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);

        // close hw disparity if sw disparity is on
        // if(value.intValue == 1) {
        //     OBPropertyValue hwDisparityValue;
        //     auto            commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        //     commandPort->getPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, &hwDisparityValue);
        //     if(hwDisparityValue.intValue == 1) {
        //         hwDisparityValue.intValue = 0;
        //         commandPort->setPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, hwDisparityValue);
        //         hwDisparityToDepthEnabled_ = false;
        //     }
        // }
        swDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);
        // update convert output frame as disparity frame
        markOutputDisparityFrame(!hwDisparityToDepthEnabled_);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto propertyAccessor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propertyAccessor->setPropertyValue(propertyId, value);
        hwDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);

        // // update sw disparity status
        // OBPropertyValue swDisparityValue;
        // auto            processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        // processor->getPropertyValue(propertyId, &swDisparityValue);
        // swDisparityValue.intValue = value.intValue == 1 ? 0 : 1;
        // processor->setPropertyValue(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL, swDisparityValue);
        // swDisparityToDepthEnabled_ = static_cast<bool>(swDisparityValue.intValue);

        // update convert output frame as disparity frame
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
            sensor->setDepthUnit(value.floatValue);
        }

    } break;

    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void Astra2Disp2DepthPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
        swDisparityToDepthEnabled_ = static_cast<bool>(value->intValue);
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

void Astra2Disp2DepthPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
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

void Astra2Disp2DepthPropertyAccessor::markOutputDisparityFrame(bool enable) {
    auto sensor = owner_->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
    if(sensor) {
        sensor->markOutputDisparityFrame(enable);
    }
}

Astra2TempPropertyAccessor::Astra2TempPropertyAccessor(IDevice *owner) : owner_(owner) {}

void Astra2TempPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(propertyId);
    utils::unusedVar(data);
    throw unsupported_operation_exception("Temperature params readonly!");
}

const std::vector<uint8_t> &Astra2TempPropertyAccessor::getStructureData(uint32_t propertyId) {
    utils::unusedVar(propertyId);
    tempData_.resize(sizeof(OBDeviceTemperature));

    auto                       commandPort = owner_->getComponentT<IStructureDataAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    const std::vector<uint8_t> tempData    = commandPort->getStructureData(OB_STRUCT_DEVICE_TEMPERATURE);
    OBDeviceTemperature        tempParam;
    memcpy(&tempParam, tempData.data(), sizeof(OBDeviceTemperature));

    tempParam.irLeftTemp     = 0;
    tempParam.irRightTemp    = 0;
    tempParam.chipBottomTemp = 0;
    tempParam.chipTopTemp    = 0;

    memcpy(tempData_.data(), &tempParam, sizeof(OBDeviceTemperature));

    return tempData_;
}

}  // namespace libobsensor