#include "UvcPropertyAccessor.hpp"
#include "usb/uvc/UvcDevicePort.hpp"

namespace libobsensor {

OBPropertyRange UvcCtrlRangeToPropRange(const UvcControlRange &ran) {
    OBPropertyRange range;
    range.max.intValue  = *(int32_t *)ran.max.data();
    range.min.intValue  = *(int32_t *)ran.min.data();
    range.def.intValue  = *(int32_t *)ran.def.data();
    range.step.intValue = *(int32_t *)ran.step.data();
    return range;
}

uint32_t convertToUvcCompatibleID(uint32_t propertyId) {
    switch(propertyId) {
    case OB_PROP_COLOR_AUTO_EXPOSURE_BOOL:
    case OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL:
    case OB_PROP_IR_AUTO_EXPOSURE_BOOL:
        return OB_PROP_COLOR_AUTO_EXPOSURE_BOOL;
    case OB_PROP_COLOR_EXPOSURE_INT:
    case OB_PROP_DEPTH_EXPOSURE_INT:
    case OB_PROP_IR_EXPOSURE_INT:
        return OB_PROP_COLOR_EXPOSURE_INT;
    case OB_PROP_COLOR_GAIN_INT:
    case OB_PROP_DEPTH_GAIN_INT:
    case OB_PROP_IR_GAIN_INT:
        return OB_PROP_COLOR_GAIN_INT;
    case OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL:
    case OB_PROP_COLOR_WHITE_BALANCE_INT:
    case OB_PROP_COLOR_BRIGHTNESS_INT:
    case OB_PROP_COLOR_SHARPNESS_INT:
    case OB_PROP_COLOR_SATURATION_INT:
    case OB_PROP_COLOR_CONTRAST_INT:
    case OB_PROP_COLOR_GAMMA_INT:
    case OB_PROP_COLOR_ROLL_INT:
    case OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT:
    case OB_PROP_COLOR_FOCUS_INT:
    case OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT:
    case OB_PROP_COLOR_HUE_INT:
    case OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT:
    case OB_PROP_IR_BRIGHTNESS_INT:
    case OB_PROP_COLOR_AE_MAX_EXPOSURE_INT:  // todo：not supported yet
    case OB_PROP_IR_AE_MAX_EXPOSURE_INT:
    case OB_PROP_COLOR_HDR_BOOL:
        return propertyId;

    default:
        throw unsupported_operation_exception("Unsupported property requestId!");
    }
}

UvcPropertyAccessor::UvcPropertyAccessor(const std::shared_ptr<ISourcePort> &backend) : backend_(backend) {
    auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(backend_);
    if(!uvcPort) {
        throw invalid_value_exception("UvcPropertyAccessor backend must be UvcDevicePort");
    }
}

void UvcPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    auto    uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(backend_);
    int32_t val;
    OBPropertyValue fixValue;

    switch(propertyId) {
    case OB_PROP_COLOR_MIRROR_BOOL: {
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        fixValue.intValue = (val & 0xFFFFFFFE) | (value.intValue & 0x01);
        if(val != fixValue.intValue) {
            uvcDevicePort->setPu(OB_PROP_COLOR_ROLL_INT, fixValue.intValue);
        }
        break;
    }
    case OB_PROP_COLOR_FLIP_BOOL: {
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        fixValue.intValue = (val & 0xFFFFFFFD) | ((value.intValue & 0x01) << 1);
        if(val != fixValue.intValue) {
            uvcDevicePort->setPu(OB_PROP_COLOR_ROLL_INT, fixValue.intValue);
        }
        break;
    }
    case OB_PROP_COLOR_FOCUS_INT: {
        // OB_PROP_COLOR_FOCUS_INT default:1, custom may be set 0 for astra+
        fixValue.intValue = value.intValue & 0x01;
        if((0 == fixValue.intValue) || (1 == fixValue.intValue)) {
            LOG_DEBUG("-OB_PROP_COLOR_FOCUS_INT setPu value:{}", fixValue.intValue);
            propertyId = convertToUvcCompatibleID(propertyId);
            uvcDevicePort->setPu(propertyId, value.intValue);
        }
        break;
    }
    default:
        propertyId = convertToUvcCompatibleID(propertyId);
        uvcDevicePort->setPu(propertyId, value.intValue);
        break;
    }
}

void UvcPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto    uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(backend_);
    int32_t val;

    switch(propertyId) {
    case OB_PROP_COLOR_MIRROR_BOOL: {
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        value->intValue = val & 0x01;
        break;
    }
    case OB_PROP_COLOR_FLIP_BOOL: {
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        value->intValue = val & (0x01 << 1);
        break;
    }
    case OB_PROP_COLOR_FOCUS_INT: {
        // OB_PROP_COLOR_FOCUS_INT default:1, custom may be set 0 for astra+
        propertyId = convertToUvcCompatibleID(propertyId);
        uvcDevicePort->getPu(propertyId, val);
        value->intValue = val & 0x01;
        LOG_DEBUG("-OB_PROP_COLOR_FOCUS_INT getPu value:{}", value->intValue);
        break;
    }
    default:
        propertyId = convertToUvcCompatibleID(propertyId);
        uvcDevicePort->getPu(propertyId, val);
        value->intValue = val;
        break;
    }
}

void UvcPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto            uvcDevicePort = std::dynamic_pointer_cast<UvcDevicePort>(backend_);
    int32_t         val;
    UvcControlRange rangeControl;

    switch(propertyId) {
    case OB_PROP_COLOR_MIRROR_BOOL: {
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        rangeControl        = uvcDevicePort->getPuRange(OB_PROP_COLOR_ROLL_INT);
        *range              = UvcCtrlRangeToPropRange(rangeControl);
        range->def.intValue = range->def.intValue & 0x01;
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        range->cur.intValue = val & 0x01;

        break;
    }
    case OB_PROP_COLOR_FLIP_BOOL: {
        // OB_PROP_COLOR_ROLL_INT：0->none 1->mirror 2->flip 3->mirror with flip
        rangeControl        = uvcDevicePort->getPuRange(OB_PROP_COLOR_ROLL_INT);
        *range              = UvcCtrlRangeToPropRange(rangeControl);
        range->def.intValue = range->def.intValue & (0x01 << 1);
        uvcDevicePort->getPu(OB_PROP_COLOR_ROLL_INT, val);
        range->cur.intValue = val & (0x01 << 1);
        break;
    }
    default: {
        propertyId   = convertToUvcCompatibleID(propertyId);
        rangeControl = uvcDevicePort->getPuRange(propertyId);
        *range       = UvcCtrlRangeToPropRange(rangeControl);
        uvcDevicePort->getPu(propertyId, val);
        range->cur.intValue = val;
        break;
    }
    }
}

}  // namespace libobsensor