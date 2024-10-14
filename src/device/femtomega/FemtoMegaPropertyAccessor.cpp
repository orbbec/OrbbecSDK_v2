// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FemtoMegaPropertyAccessor.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include "IDeviceComponent.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {
FemtoMegaTempPropertyAccessor::FemtoMegaTempPropertyAccessor(IDevice *owner) : owner_(owner) {}

void FemtoMegaTempPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    utils::unusedVar(propertyId);
    utils::unusedVar(data);
    throw unsupported_operation_exception("Temperature params readonly!");
}

const std::vector<uint8_t> &FemtoMegaTempPropertyAccessor::getStructureData(uint32_t propertyId) {
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
