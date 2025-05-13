// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G2PropertyAccessors.hpp"
#include "component/frameprocessor/FrameProcessor.hpp"
#include "gemini2/G2DepthWorkModeManager.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

G2Disp2DepthPropertyAccessor::G2Disp2DepthPropertyAccessor(IDevice *owner) : hwDisparityToDepthEnabled_(true), owner_(owner) {}

IDevice *G2Disp2DepthPropertyAccessor::getOwner() {
    return owner_;
}

void G2Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    owner_->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
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

G2FrameTransformPropertyAccessor::G2FrameTransformPropertyAccessor(IDevice *owner) : owner_(owner), optionCode_(INVALID) {}

OBDepthModeOptionCode G2FrameTransformPropertyAccessor::getOptionCode() {
    // lazy initialization of option code, case G2DepthWorkModeManager is lazy initialized
    if(optionCode_ == INVALID) {
        auto depthWorkModeManager = owner_->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
        optionCode_               = static_cast<OBDepthModeOptionCode>(depthWorkModeManager->getCurrentDepthWorkMode().optionCode);
    }

    return optionCode_;
}

void G2FrameTransformPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        if(getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("Depth properties unavailable in MX6600 mode");
            return;
        }

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        const auto componentId =
            getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL ? OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR : OB_DEV_COMPONENT_IR_FRAME_PROCESSOR;

        auto processor = owner_->getComponentT<FrameProcessor>(componentId);
        if(propertyId == OB_PROP_IR_MIRROR_BOOL) {
            OBPropertyValue correctValue = value;
            correctValue.intValue        = !correctValue.intValue;
            processor->setPropertyValue(propertyId, correctValue);
        }
        else {
            processor->setPropertyValue(propertyId, value);
        }
    } break;
    case OB_PROP_IR_RIGHT_MIRROR_BOOL:
    case OB_PROP_IR_RIGHT_FLIP_BOOL:
    case OB_PROP_IR_RIGHT_ROTATE_INT: {
        if(getOptionCode() != MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("IR right properties require MX6600 mode");
            return;
        }

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);
        if(propertyId == OB_PROP_IR_RIGHT_MIRROR_BOOL) {
            OBPropertyValue correctValue = value;
            correctValue.intValue        = !correctValue.intValue;
            processor->setPropertyValue(propertyId, correctValue);
        }
        else {
            processor->setPropertyValue(propertyId, value);
        }
    } break;
    default: {
        throw invalid_value_exception("Invalid property id");
    }
    }
}

void G2FrameTransformPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    if(!value) {
        LOG_DEBUG("Null value pointer, abort property access");
        return;
    }
    memset(value, 0, sizeof(OBPropertyValue));

    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        if(getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("Depth properties unavailable in MX6600 Right IR mode");
            return;
        }
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        const auto component =
            getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL ? OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR : OB_DEV_COMPONENT_IR_FRAME_PROCESSOR;

        auto processor = owner_->getComponentT<FrameProcessor>(component);
        processor->getPropertyValue(propertyId, value);

        if(propertyId == OB_PROP_IR_MIRROR_BOOL) {
            value->intValue = !value->intValue;  // frame capture from hardware is always mirrored, so we need to flip it
        }
    } break;
    case OB_PROP_IR_RIGHT_MIRROR_BOOL:
    case OB_PROP_IR_RIGHT_FLIP_BOOL:
    case OB_PROP_IR_RIGHT_ROTATE_INT: {
        if(getOptionCode() != MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("IR right properties require MX6600 Right IR mode");
            return;
        }

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);

        if(propertyId == OB_PROP_IR_RIGHT_MIRROR_BOOL) {
            value->intValue = !value->intValue;  // frame capture from hardware is always mirrored, so we need to flip it
        }
    } break;
    default: {
        throw invalid_value_exception("Invalid property identifier");
    }
    }
}

void G2FrameTransformPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    if(range == nullptr) {
        LOG_DEBUG("Null range pointer, abort property access");
        return;
    }

    memset(range, 0, sizeof(OBPropertyRange));

    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        if(getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("Depth properties unavailable in MX6600 mode");
            return;
        }

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        const auto componentId =
            getOptionCode() == MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL ? OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR : OB_DEV_COMPONENT_IR_FRAME_PROCESSOR;

        auto processor = owner_->getComponentT<FrameProcessor>(componentId);
        processor->getPropertyRange(propertyId, range);

        if(propertyId == OB_PROP_IR_MIRROR_BOOL) {
            range->cur.intValue = !range->cur.intValue;
        }
    } break;
    case OB_PROP_IR_RIGHT_MIRROR_BOOL:
    case OB_PROP_IR_RIGHT_FLIP_BOOL:
    case OB_PROP_IR_RIGHT_ROTATE_INT: {
        if(getOptionCode() != MX6600_RIGHT_IR_FROM_DEPTH_CHANNEL) {
            LOG_DEBUG("IR right properties require MX6600 Right IR mode");
            return;
        }

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);

        if(propertyId == OB_PROP_IR_RIGHT_MIRROR_BOOL) {
            range->cur.intValue = !range->cur.intValue;
        }
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

G435LeDisp2DepthPropertyAccessor::G435LeDisp2DepthPropertyAccessor(IDevice *owner) : G2Disp2DepthPropertyAccessor(owner) {
    hwD2DSupportList_ = { OB_PRECISION_1MM, OB_PRECISION_0MM4, OB_PRECISION_0MM2 };
}

const std::vector<uint8_t> &G435LeDisp2DepthPropertyAccessor::getStructureData(uint32_t propertyId) {
    if(propertyId == OB_STRUCT_DEPTH_PRECISION_SUPPORT_LIST) {
        if(hwDisparityToDepthEnabled_) {
            auto owner       = getOwner();
            auto commandPort = owner->getComponentT<IStructureDataAccessorV1_1>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            return commandPort->getStructureDataProtoV1_1(propertyId, 0);
        }
        else {
            return G2Disp2DepthPropertyAccessor::getStructureData(propertyId);
        }
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

G210FrameTransformPropertyAccessor::G210FrameTransformPropertyAccessor(IDevice *owner) : owner_(owner) {}

void G210FrameTransformPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR);

        if (propertyId == OB_PROP_IR_MIRROR_BOOL) {
            OBPropertyValue correctValue = value;
            correctValue.intValue        = !correctValue.intValue;
            processor->setPropertyValue(propertyId, correctValue);
        }
        else {
            processor->setPropertyValue(propertyId, value);
        }
    } break;
    default: {
        throw invalid_value_exception("Invalid property id");
    }
    }
}

void G210FrameTransformPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    if(!value) {
        LOG_DEBUG("Null value pointer, abort property access");
        return;
    }
    memset(value, 0, sizeof(OBPropertyValue));

    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR);
        processor->getPropertyValue(propertyId, value);

        if (propertyId == OB_PROP_IR_MIRROR_BOOL) {
            value->intValue = !value->intValue;
        }
    } break;
    default: {
        throw invalid_value_exception("Invalid property id");
    }
    }
}

void G210FrameTransformPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    if(range == nullptr) {
        LOG_DEBUG("Null range pointer, abort property access");
        return;
    }
    memset(range, 0, sizeof(OBPropertyRange));

    switch(propertyId) {
    case OB_PROP_DEPTH_MIRROR_BOOL:
    case OB_PROP_DEPTH_FLIP_BOOL:
    case OB_PROP_DEPTH_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_COLOR_MIRROR_BOOL:
    case OB_PROP_COLOR_FLIP_BOOL:
    case OB_PROP_COLOR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);
    } break;
    case OB_PROP_IR_MIRROR_BOOL:
    case OB_PROP_IR_FLIP_BOOL:
    case OB_PROP_IR_ROTATE_INT: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_IR_FRAME_PROCESSOR);
        processor->getPropertyRange(propertyId, range);

        if(propertyId == OB_PROP_IR_MIRROR_BOOL) {
            range->cur.intValue = !range->cur.intValue;
        }
    } break;
    default: {
        throw invalid_value_exception("Invalid property id");
    }
    }
}
}  // namespace libobsensor
