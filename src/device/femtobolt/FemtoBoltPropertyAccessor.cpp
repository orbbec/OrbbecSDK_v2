#include "FemtoBoltPropertyAccessor.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"
#include "property/InternalProperty.hpp"
#include "rawphase/RawPhaseStreamer.hpp"
#include "rawphase/RawPhaseBasedSensor.hpp"

namespace libobsensor {
FemtoBoltIrModePropertyAccessor::FemtoBoltIrModePropertyAccessor(IDevice *owner) : owner_(owner) {
    // OBPropertyValue value;
    // value.intValue = 0;
    // setPropertyValue(OB_PROP_SWITCH_IR_MODE_INT, value);
}

void FemtoBoltIrModePropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->setPropertyValue(propertyId, value);

    if(propertyId == OB_PROP_SWITCH_IR_MODE_INT) {
        auto rawPhaseStreamer = owner_->getComponentT<RawPhaseStreamer>(OB_DEV_COMPONENT_RAW_PHASE_STREAMER);
        rawPhaseStreamer->enablePassiveIRMode((bool)value.intValue);

        auto depthSensor = owner_->getComponentT<RawPhaseBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR);
        depthSensor->refreshStreamProfiles();

        auto irSensor = owner_->getComponentT<RawPhaseBasedSensor>(OB_DEV_COMPONENT_IR_SENSOR);
        irSensor->refreshStreamProfiles();
    }
}

void FemtoBoltIrModePropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyValue(propertyId, value);
}

void FemtoBoltIrModePropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    auto commandPort = owner_->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    commandPort->getPropertyRange(propertyId, range);
}

FemtoBoltTempPropertyAccessor::FemtoBoltTempPropertyAccessor(IDevice *owner) : owner_(owner) {}

void FemtoBoltTempPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(propertyId);
    utils::unusedVar(data);
    throw unsupported_operation_exception("Temperature params readonly!");
}

const std::vector<uint8_t> &FemtoBoltTempPropertyAccessor::getStructureData(uint32_t propertyId) {
    utils::unusedVar(propertyId);
    tempData_.resize(sizeof(OBDeviceTemperature));

    auto                       commandPort = owner_->getComponentT<IStructureDataAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    const std::vector<uint8_t> tempData    = commandPort->getStructureData(OB_STRUCT_DEVICE_TEMPERATURE);
    OBDeviceTemperature        tempParam;
    memset(&tempParam, 0, sizeof(OBDeviceTemperature));
    memcpy(&tempParam, tempData.data(), sizeof(OBDeviceTemperature));

    tempParam.irLeftTemp     = 0;
    tempParam.irRightTemp    = 0;
    tempParam.chipBottomTemp = 0;
    tempParam.chipTopTemp    = 0;
    tempParam.mainBoardTemp  = 0;
    tempParam.rgbTemp        = 0;
    tempParam.tecTemp        = 0;

    memcpy(tempData_.data(), &tempParam, sizeof(OBDeviceTemperature));

    return tempData_;
}

}  // namespace libobsensor