// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330PropertyAccessors.hpp"
#include "component/frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"
#include "G330NetStreamProfileFilter.hpp"
#include "IDeviceClockSynchronizer.hpp"

namespace libobsensor {

G330Disp2DepthPropertyAccessor::G330Disp2DepthPropertyAccessor(IDevice *owner) : owner_(owner), hwDisparityToDepthEnabled_(true) {}

void G330Disp2DepthPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL: {
        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
        hwDisparityToDepthEnabled_ = static_cast<bool>(value.intValue);

        // update convert output frame as disparity frame
        markOutputDisparityFrame(!hwDisparityToDepthEnabled_);

        auto processor = owner_->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR);
        processor->setPropertyValue(propertyId, value);
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
    case OB_PROP_DISP_SEARCH_OFFSET_INT: {
        auto sensor = owner_->getComponentT<ISensor>(OB_DEV_COMPONENT_DEPTH_SENSOR).get();
        if(!sensor->isStreamActivated()) {
            throw libobsensor::wrong_api_call_sequence_exception("disp search offset can only be set when depth sensor is activated");
        }

        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
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
    } break;
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(OB_PROP_DISPARITY_TO_DEPTH_BOOL, value);
        hwDisparityToDepthEnabled_ = static_cast<bool>(value->intValue);
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
    case OB_PROP_DISP_SEARCH_OFFSET_INT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(OB_PROP_DISP_SEARCH_OFFSET_INT, value);
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
    case OB_PROP_DISPARITY_TO_DEPTH_BOOL: {
        range->min.intValue  = 0;
        range->max.intValue  = 1;
        range->step.intValue = 1;
        range->def.intValue  = 1;
        range->cur.intValue  = static_cast<int32_t>(hwDisparityToDepthEnabled_);
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
    case OB_PROP_DISP_SEARCH_OFFSET_INT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyRange(propertyId, range);
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

G330NetPerformanceModePropertyAccessor::G330NetPerformanceModePropertyAccessor(IDevice *owner) : owner_(owner) {}

void G330NetPerformanceModePropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_DEVICE_PERFORMANCE_MODE_INT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(OB_PROP_DEVICE_PERFORMANCE_MODE_INT, value);
        performanceMode_ = value.intValue;

        // update performance mode
        updatePerformanceMode(performanceMode_);
    } break;
    default: {
    } break;
    }
}

void G330NetPerformanceModePropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_DEVICE_PERFORMANCE_MODE_INT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(OB_PROP_DEVICE_PERFORMANCE_MODE_INT, value);
        performanceMode_ = value->intValue;
    } break;
    default: {
    } break;
    }
}

void G330NetPerformanceModePropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_DEVICE_PERFORMANCE_MODE_INT: {
        range->min.intValue  = 0;
        range->max.intValue  = 1;
        range->step.intValue = 1;
        range->def.intValue  = 1;
        range->cur.intValue  = performanceMode_;
    } break;
    default: {
    } break;
    }
}

void G330NetPerformanceModePropertyAccessor::updatePerformanceMode(uint32_t mode) {
    auto streamProfileFilter = owner_->getComponentT<G330NetStreamProfileFilter>(OB_DEV_COMPONENT_STREAM_PROFILE_FILTER);
    streamProfileFilter->switchFilterMode((OBCameraPerformanceMode)mode);
}

G330HWNoiseRemovePropertyAccessor::G330HWNoiseRemovePropertyAccessor(IDevice *owner) : owner_(owner) {}

void G330HWNoiseRemovePropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL: {
        auto processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        processor->setPropertyValue(propertyId, value);
    } break;
    case OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT: {
        auto processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        // due to the firmware not supporting the log function, the log function conversion is done in the SDK.
        auto            threhold = log(value.floatValue / (1 - value.floatValue));
        OBPropertyValue hwNoiseRemoveFilterThreshold;
        hwNoiseRemoveFilterThreshold.floatValue = threhold;
        processor->setPropertyValue(propertyId, hwNoiseRemoveFilterThreshold);
    } break;

    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void G330HWNoiseRemovePropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL: {
        auto processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        processor->getPropertyValue(propertyId, value);
    } break;
    case OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT: {
        auto            processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        OBPropertyValue hwNoiseRemoveFilterThreshold;
        processor->getPropertyValue(propertyId, &hwNoiseRemoveFilterThreshold);
        auto threshold    = exp(hwNoiseRemoveFilterThreshold.floatValue) / (1 + exp(hwNoiseRemoveFilterThreshold.floatValue));
        value->floatValue = threshold;

    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyValue(propertyId, value);
    } break;
    }
}

void G330HWNoiseRemovePropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyRange(propertyId, range);
        auto cur              = exp(range->cur.floatValue) / (1 + exp(range->cur.floatValue));
        range->cur.floatValue = cur;
    } break;

    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->getPropertyRange(propertyId, range);
    } break;
    }
}


G330NetPTPClockSyncPropertyAccessor::G330NetPTPClockSyncPropertyAccessor(IDevice *owner) : owner_(owner) {}

void G330NetPTPClockSyncPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_DEVICE_PTP_CLOCK_SYNC_ENABLE_BOOL: {
        if(value.intValue == 0) {
            auto processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            processor->setPropertyValue(propertyId, value);
        }
        else {
            auto deviceClockSync = owner_->getComponentT<IDeviceClockSynchronizer>(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
            deviceClockSync->timerSyncWithHost();
            auto processor = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            processor->setPropertyValue(propertyId, value);
        }
    } break;
    default: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(propertyId, value);
    } break;
    }
}

void G330NetPTPClockSyncPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyValue(propertyId, value);
}

void G330NetPTPClockSyncPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyRange(propertyId, range);
}
}  // namespace libobsensor
