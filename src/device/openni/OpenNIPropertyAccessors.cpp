// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIPropertyAccessors.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "OpenNIDisparitySensor.hpp"
#include "IDeviceComponent.hpp"
#include "component/property/InternalProperty.hpp"
#include <limits>

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
        auto sensor = owner_->getComponentT<OpenNIDisparitySensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
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
    auto sensor = owner_->getComponentT<OpenNIDisparitySensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
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



OpenNIFrameTransformPropertyAccessor::OpenNIFrameTransformPropertyAccessor(IDevice *owner) : owner_(owner) {}

void OpenNIFrameTransformPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
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
        processor->setPropertyValue(propertyId, value);
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void OpenNIFrameTransformPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
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
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void OpenNIFrameTransformPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
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
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}


OpenNIHeartBeatPropertyAccessor::OpenNIHeartBeatPropertyAccessor(IDevice *owner)
    : owner_(owner),
      heartBeatStatus_(false),
      heartBeatRunning_(false){
    BEGIN_TRY_EXECUTE({
          OBPropertyValue value;
          auto            commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
          commandPort->getPropertyValue(OB_PROP_WATCHDOG_BOOL, &value);
          if(value.intValue == 1) {
              heartBeatStatus_ = true;
              // start watch dog
              startFeedingWatchDog();
          }
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get watch dog status failed!");
    })
}

OpenNIHeartBeatPropertyAccessor::~OpenNIHeartBeatPropertyAccessor() noexcept {
    stopFeedingWatchDog();
    BEGIN_TRY_EXECUTE({
        if(heartBeatStatus_) {
            OBPropertyValue value;
            value.intValue   = 0;
            auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->setPropertyValue(OB_PROP_WATCHDOG_BOOL, value);
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ 
        LOG_WARN("Set watch dog status failed!");
    })
}

void OpenNIHeartBeatPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    switch(propertyId) {
    case OB_PROP_HEARTBEAT_BOOL: {
        auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        commandPort->setPropertyValue(OB_PROP_WATCHDOG_BOOL, value);
        heartBeatStatus_ = value.intValue == 1 ? true : false;
        if(value.intValue == 1) {
            startFeedingWatchDog();
        }
        else {
            stopFeedingWatchDog();
        }
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void OpenNIHeartBeatPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch(propertyId) {
    case OB_PROP_HEARTBEAT_BOOL: {
        value->intValue = heartBeatStatus_ ? 1 : 0;
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void OpenNIHeartBeatPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    switch(propertyId) {
    case OB_PROP_HEARTBEAT_BOOL: {
        range->cur.intValue  = heartBeatStatus_ ? 1 : 0;
        range->def.intValue  = 0;
        range->min.intValue  = 0;
        range->max.intValue  = 1;
        range->step.intValue = 1;
    } break;
    default:
        throw invalid_value_exception("Invalid property id");
    }
}

void OpenNIHeartBeatPropertyAccessor::startFeedingWatchDog() {
    heartBeatRunning_ = true;
    heartBeatThread_  = std::thread([&]() {
        LOG_INFO("Start feading watchdog.");
        while(heartBeatRunning_) {
            std::unique_lock<std::mutex> lock(heartBeatMutex_);
            OBPropertyValue              value;
            value.intValue = 1;
            auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
            commandPort->setPropertyValue(OB_PROP_DEVICE_KEEP_ALIVE_INT, value);
            heartBeatCV_.wait_for(lock, std::chrono::milliseconds(OB_DEFAULT_HEARTBEAT_TIMEOUT));
        }

        LOG_INFO("Exit feading watchdog.");
    });
}

void OpenNIHeartBeatPropertyAccessor::stopFeedingWatchDog() {
    if(heartBeatRunning_) {
        LOG_INFO("Stop feading watchdog.");
        heartBeatRunning_ = false;
        heartBeatCV_.notify_all();
        heartBeatThread_.join();
    }
}



OpenNITemperatureStructurePropertyAccessor::OpenNITemperatureStructurePropertyAccessor(IDevice *owner)
    : owner_(owner){
}

void OpenNITemperatureStructurePropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(data);
    if(propertyId == OB_STRUCT_DEVICE_TEMPERATURE) {
        throw invalid_value_exception("OB_PROP_TEMPERATURE_COMPENSATION_BOOL is read-only");
    }
    throw invalid_value_exception("OB_PROP_TEMPERATURE_COMPENSATION_BOOL is read-only");
}

const std::vector<uint8_t> &OpenNITemperatureStructurePropertyAccessor::getStructureData(uint32_t propertyId) {
    if(propertyId == OB_STRUCT_DEVICE_TEMPERATURE) {
        auto                commandPort = owner_->getComponentT<IStructureDataAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        auto                data        = commandPort->getStructureData(OB_STRUCT_DEVICE_TEMPERATURE);
        float               fNaN        = std::numeric_limits<float>::quiet_NaN();
        OBDeviceTemperature deviceTemp  = { fNaN, fNaN, fNaN, fNaN, fNaN, fNaN, fNaN, fNaN, fNaN, fNaN, fNaN };
        memcpy(&deviceTemp, data.data(), data.size());
        static std::vector<uint8_t> result(sizeof(OBDeviceTemperature));
        memcpy(result.data(), &deviceTemp, sizeof(OBDeviceTemperature));
        return result;
    }
    throw invalid_value_exception(utils::string::to_string() << "unsupported property id:" << propertyId);
}

}  // namespace libobsensor
