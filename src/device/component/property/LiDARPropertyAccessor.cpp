// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARPropertyAccessor.hpp"
#include "exception/ObException.hpp"
#include "protocol/LiDARProtocol.hpp"
#include "libobsensor/h/Property.h"
#include "InternalProperty.hpp"

namespace libobsensor {

using HpOpCode = libobsensor::lidarprotocol::HpOpCode;

typedef struct LiDAROpCode {
    HpOpCode opCodeSet;
    HpOpCode opCodeGet;
} LiDAROpCode;

const std::unordered_map<uint32_t, LiDAROpCode> LiDAROperationInfomap = {
    { OB_RAW_DATA_LIDAR_IP_ADDRESS, { HpOpCode::OPCODE_SET_IP_ADDR, HpOpCode::OPCODE_GET_IP_ADDR } },                          // set/get ip address
    { OB_PROP_LIDAR_PORT_INT, { HpOpCode::OPCODE_SET_PORT, HpOpCode::OPCODE_GET_PORT } },                          // set/get port
    { OB_RAW_DATA_LIDAR_MAC_ADDRESS, { HpOpCode::OPCODE_SET_MAC_ADDR, HpOpCode::OPCODE_GET_MAC_ADDR } },                          // set/get mac address
    { OB_RAW_DATA_LIDAR_SUBNET_MASK, { HpOpCode::OPCODE_SET_SUBNET_MASK, HpOpCode::OPCODE_GET_SUBNET_MASK } },                    // set/get subnet mask
    { OB_PROP_LIDAR_SCAN_SPEED_INT, { HpOpCode::OPCODE_SET_SCAN_SPEED, HpOpCode::OPCODE_GET_SCAN_SPEED } },                       // set/get scan speed
    { OB_PROP_LIDAR_SCAN_DIRECTION_INT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_SCAN_DIRECTION } },           // set/get scan direction
    { OB_PROP_LIDAR_TRANSFER_PROTOCOL_INT, { HpOpCode::OPCODE_SET_TRANSFER_PROTOCOL, HpOpCode::OPCODE_GET_TRANSFER_PROTOCOL } },  // set/get transfer protocol
    { OB_PROP_LIDAR_WORK_MODE_INT, { HpOpCode::OPCODE_SET_WORK_MODE, HpOpCode::OPCODE_GET_WORK_MODE } },                          // set/get work mode
    { OB_PROP_LIDAR_INITIATE_DEVICE_CONNECTION_INT,
      { HpOpCode::OPCODE_INITIATE_DEVICE_CONNECTION, HpOpCode::OPCODE_UNSUPPORTED } },                                            // initiate device connection
    { OB_RAW_DATA_LIDAR_SERIAL_NUMBER, { HpOpCode::OPCODE_SET_SERIAL_NUMBER, HpOpCode::OPCODE_GET_SERIAL_NUMBER } },              // set/get serial number
    { OB_PROP_REBOOT_DEVICE_BOOL, { HpOpCode::OPCODE_REBOOT_DEVICE, HpOpCode::OPCODE_UNSUPPORTED } },                             // reboot device
    { OB_PROP_LIDAR_ECHO_MODE_INT, { HpOpCode::OPCODE_SET_ECHO_MODE, HpOpCode::OPCODE_GET_ECHO_MODE } },                          // set/get echo mode
    { OB_PROP_LIDAR_APPLY_CONFIGS_INT, { HpOpCode::OPCODE_APPLY_CONFIGS, HpOpCode::OPCODE_UNSUPPORTED } },                        // apply configs
    { OB_PROP_LIDAR_STREAMING_ON_OFF_INT, { HpOpCode::OPCODE_STREAMING_ON_OFF, HpOpCode::OPCODE_UNSUPPORTED } },                  // streaming on/off
    { OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, { HpOpCode::OPCODE_SET_TAIL_FILTER_LEVEL, HpOpCode::OPCODE_GET_TAIL_FILTER_LEVEL } },  // set/set tail filter level
    { OB_PROP_LIDAR_MEMS_FOV_SIZE_FLOAT, { HpOpCode::OPCODE_SET_MEMS_FOV_SIZE, HpOpCode::OPCODE_GET_MEMS_FOV_SIZE } },              // set/get mems fov size
    { OB_PROP_LIDAR_MEMS_FRENQUENCY_FLOAT, { HpOpCode::OPCODE_SET_MEMS_FRENQUENCY, HpOpCode::OPCODE_GET_MEMS_FRENQUENCY } },        // set/get mems frequency
    { OB_PROP_LIDAR_MEMS_FOV_FACTOR_FLOAT, { HpOpCode::OPCODE_SET_MEMS_FOV_FACTOR, HpOpCode::OPCODE_GET_MEMS_FOV_FACTOR } },        // set/get mems fov factor
    { OB_PROP_LIDAR_MEMS_ON_OFF_INT, { HpOpCode::OPCODE_MEMS_ON_OFF, HpOpCode::OPCODE_UNSUPPORTED } },                            // mems on/off
    { OB_PROP_LIDAR_RESTART_MEMS_INT, { HpOpCode::OPCODE_RESTART_MEMS, HpOpCode::OPCODE_UNSUPPORTED } },                          // restart mems
    { OB_PROP_LIDAR_SAVE_MEMS_PARAM_INT, { HpOpCode::OPCODE_SAVE_MEMS_PARAM, HpOpCode::OPCODE_UNSUPPORTED } },                    // save mems param

    { OB_RAW_DATA_LIDAR_PRODUCT_MODEL, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_PRODUCT_MODEL } },                    // get product model
    { OB_RAW_DATA_LIDAR_FIRMWARE_VERSION, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_FIRMWARE_VERSION } },              // get firmware version
    { OB_RAW_DATA_LIDAR_FPGA_VERSION, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_FPGA_VERSION } },                      // get fpga version
    { OB_STRUCT_LIDAR_STATUS_INFO, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_STATUS_INFO } },                          // get status info
    { OB_PROP_LIDAR_WARNING_INFO_INT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_WARNING_INFO } },                      // get warning info
    { OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_MOTOR_SPIN_SPEED } },              // get spin speed
    { OB_PROP_LIDAR_MCU_TEMPERATURE_FLOAT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_MCU_TEMPERATURE } },              // get mcu temperature
    { OB_PROP_LIDAR_FPGA_TEMPERATURE_FLOAT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_FPGA_TEMPERATURE } },            // get fpga temperature
    { OB_RAW_DATA_LIDAR_MOTOR_VERSION, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_MOTOR_VERSION } },                    // get motor version
    { OB_PROP_LIDAR_APD_HIGH_VOLTAGE_FLOAT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_APD_HIGH_VOLTAGE } },            // get apd high voltage
    { OB_PROP_LIDAR_APD_TEMPERATURE_FLOAT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_APD_TEMPERATURE } },              // get apd temperature
    { OB_PROP_LIDAR_TX_HIGH_POWER_VOLTAGE_FLOAT, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_TX_HIGH_POWER_VOLTAGE } },  // get tx high power voltage
    { OB_PROP_LIDAR_TX_LOWER_POWER_VOLTAGE_FLOAT,
      { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_TX_LOWER_POWER_VOLTAGE } },                        // get tx lower power voltage
    { OB_RAW_DATA_LIDAR_MEMS_VERSION, { HpOpCode::OPCODE_UNSUPPORTED, HpOpCode::OPCODE_GET_MEMS_VERSION } },  // get mems version
};

LiDARPropertyAccessor::LiDARPropertyAccessor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend)
    : owner_(owner), backend_(backend), recvData_(2048), sendData_(1024) {
    auto port = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    if(!port) {
        throw invalid_value_exception("LiDARPropertyAccessor backend must be IVendorDataPort");
    }
}

void LiDARPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto opCode  = OBPropertyToOpCode(propertyId, true);
    auto reqSize = lidarprotocol::initSetIntPropertyReq(sendData_, opCode, value.intValue);

    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    uint16_t respDataSize = 0;
    auto     res          = lidarprotocol::execute(port, opCode, sendData_.data(), reqSize, recvData_.data(), &respDataSize);
    lidarprotocol::checkStatus(res);
}

void LiDARPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto opCode = OBPropertyToOpCode(propertyId, false);
    auto reqSize = lidarprotocol::initGetIntPropertyReq(sendData_, opCode);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = lidarprotocol::execute(port, opCode, sendData_.data(), reqSize, recvData_.data(), &respDataSize);

    lidarprotocol::checkStatus(res);

    auto resp       = lidarprotocol::parseGetIntPropertyResp(recvData_.data(), respDataSize);
    value->intValue = resp->value;
}

void LiDARPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();

    // get current property value
    {
        auto opCode  = OBPropertyToOpCode(propertyId, false);
        auto reqSize = lidarprotocol::initGetIntPropertyReq(sendData_, opCode);

        uint16_t respDataSize = 0;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        auto     res          = lidarprotocol::execute(port, opCode, sendData_.data(), reqSize, recvData_.data(), &respDataSize);

        lidarprotocol::checkStatus(res);

        auto resp           = lidarprotocol::parseGetIntPropertyResp(recvData_.data(), respDataSize);
        range->cur.intValue = resp->value;
    }
    // set property range
    // TODO: range value of single-line and multi-lines LiDAR may be different!
    // and some range value may be changed in the future.
    switch(propertyId) {
    case OB_PROP_LIDAR_PORT_INT: {
        range->min.intValue  = 1024;
        range->max.intValue  = 65535;
        range->step.intValue = 1;
        range->def.intValue  = 2401;
        break;
    }
    case OB_PROP_LIDAR_SCAN_SPEED_INT: {
        // TODO single-line and multi-lines LiDAR may be different!
        range->min.intValue  = 0;
        range->max.intValue  = 1200;  // 20HZ
        range->step.intValue = 300;   // 5HZ
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_SCAN_DIRECTION_INT: {
        range->min.intValue  = 0;
        range->max.intValue  = 1;
        range->step.intValue = 1;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_TRANSFER_PROTOCOL_INT: {
        range->min.intValue  = 0; // udp
        range->max.intValue  = 1; // tcp
        range->step.intValue = 1;
        range->def.intValue  = 0; // udp
        break;
    }
    case OB_PROP_LIDAR_WORK_MODE_INT: {
        range->min.intValue  = 0; // normal
        range->max.intValue  = 7;
        range->step.intValue = 1;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_ECHO_MODE_INT: {
        range->min.intValue  = 0;  // single echo
        range->max.intValue  = 1; // dual echo
        range->step.intValue = 1;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT: {
        range->min.intValue  = 0;  // close
        range->max.intValue  = 1;  // open
        range->step.intValue = 1;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_MEMS_FOV_SIZE_FLOAT: {
        range->min.floatValue  = 0.0f;
        range->max.floatValue = 60.0f;
        range->step.floatValue = 0.5f;
        range->def.floatValue  = 0.0f;
        break;
    }
    case OB_PROP_LIDAR_MEMS_FRENQUENCY_FLOAT: {
        range->min.floatValue  = 0.0f;
        range->max.floatValue  = 1100.0f;
        range->step.floatValue = 0.5f;
        range->def.floatValue  = 1100.0f;
        break;
    }
    case OB_PROP_LIDAR_MEMS_FOV_FACTOR_FLOAT: {
        range->min.floatValue  = 1.0f;
        range->max.floatValue  = 2.0f;
        range->step.floatValue = 0.01f;
        range->def.floatValue  = 1.0f;
        break;
    }
    case OB_PROP_LIDAR_WARNING_INFO_INT: {
        range->min.intValue  = 0;
        range->max.intValue  = 0x3FF;  // 10bit
        range->step.intValue = 1;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT: {
        range->min.intValue  = 0;
        range->max.intValue  = 1200;
        range->step.intValue = 300;
        range->def.intValue  = 0;
        break;
    }
    case OB_PROP_LIDAR_MCU_TEMPERATURE_FLOAT:
    case OB_PROP_LIDAR_FPGA_TEMPERATURE_FLOAT:
    case OB_PROP_LIDAR_APD_TEMPERATURE_FLOAT: {
        range->min.floatValue  = 0.0f;
        range->max.floatValue  = 100.0f;
        range->step.floatValue = 0.01f;
        range->def.floatValue  = 0.0f;
        break;
    }
    case OB_PROP_LIDAR_TX_HIGH_POWER_VOLTAGE_FLOAT:
    case OB_PROP_LIDAR_TX_LOWER_POWER_VOLTAGE_FLOAT:
    default: {
        std::stringstream ss;
        ss << "LiDARPropertyAccessor propertyId(" << propertyId << ") doesn't support get range!!!";
        throw unsupported_operation_exception(ss.str());
        break;
    }
    }
}

void LiDARPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto opCode = OBPropertyToOpCode(propertyId, true);
    auto reqSize = lidarprotocol::initSetRawDataReq(sendData_, opCode, data.data(), static_cast<uint16_t>(data.size()));

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = lidarprotocol::execute(port, opCode, sendData_.data(), reqSize, recvData_.data(), &respDataSize);
    lidarprotocol::checkStatus(res);
}

const std::vector<uint8_t> &LiDARPropertyAccessor::getStructureData(uint32_t propertyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto opCode  = OBPropertyToOpCode(propertyId, false);
    auto reqSize = lidarprotocol::initGetRawDataReq(sendData_, opCode);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = lidarprotocol::execute(port, opCode, sendData_.data(), reqSize, recvData_.data(), &respDataSize);
    lidarprotocol::checkStatus(res);

    auto resp              = lidarprotocol::parseGetRawDataResp(recvData_.data(), respDataSize);
    auto structureDataSize = lidarprotocol::getRaweDataSize(resp);
    if(structureDataSize <= 0) {
        res.statusCode    = lidarprotocol::HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE;
        res.respErrorCode = lidarprotocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data return data size invalid";
        lidarprotocol::checkStatus(res);
    }
    outputData_.resize(structureDataSize);
    memcpy(outputData_.data(), resp->data, structureDataSize);
    return outputData_;
}

uint16_t LiDARPropertyAccessor::OBPropertyToOpCode(uint32_t propertyId, bool set) {
    auto iter = LiDAROperationInfomap.find(propertyId);

    if(iter == LiDAROperationInfomap.end()) {
        throw unsupported_operation_exception("LiDARPropertyAccessor propertyId is not supported");
    }
    else {
        auto opCode = set ? iter->second.opCodeSet : iter->second.opCodeGet;
        if(opCode == lidarprotocol::HpOpCode::OPCODE_UNSUPPORTED) {
            std::stringstream ss;
            ss << "LiDARPropertyAccessor propertyId(" << propertyId << ") doesn't support ";
            auto opStr = set ? "setting" : "getting";
            ss << opStr;
            throw unsupported_operation_exception(ss.str());
        }

        return static_cast<uint16_t>(opCode);
    }
}

void LiDARPropertyAccessor::clearBuffers() {
    memset(recvData_.data(), 0, recvData_.size());
    memset(sendData_.data(), 0, sendData_.size());
}

IDevice *LiDARPropertyAccessor::getOwner() const {
    return owner_;
}

}  // namespace libobsensor
