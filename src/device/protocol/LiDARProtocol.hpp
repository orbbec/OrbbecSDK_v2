// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "IProperty.hpp"
#include <sstream>

namespace libobsensor {
namespace lidarprotocol {

// HP mean hardware protocol

enum HpOpCode {
    OPCODE_UNSUPPORTED = 0x0000,

    // scan mode set
    // set LiDAR ip address
    OPCODE_SET_IP_ADDR = 0x0101,
    // set LiDAR port
    OPCODE_SET_PORT = 0x0102,
    // set LiDAR mac address
    OPCODE_SET_MAC_ADDR = 0x0103,
    //  set LiDAR subnet mask
    OPCODE_SET_SUBNET_MASK = 0x0104,
    // set LiDAR scan speed
    OPCODE_SET_SCAN_SPEED = 0x0105,
    // 0x0106 is reserved
    // set LiDAR transfer protocol
    OPCODE_SET_TRANSFER_PROTOCOL = 0x0107,
    // set LiDAR work mode
    OPCODE_SET_WORK_MODE = 0x0108,
    // set LiDAR initiate device connection
    OPCODE_INITIATE_DEVICE_CONNECTION = 0x0109,
    // set LiDAR serial number
    OPCODE_SET_SERIAL_NUMBER = 0x010A,
    // reboot device
    OPCODE_REBOOT_DEVICE = 0x010B,
    // 0x010C is reserved
    // set LiDAR echo mode
    OPCODE_SET_ECHO_MODE = 0x010D,
    // apply configs
    OPCODE_APPLY_CONFIGS = 0x010E,
    // enable or disable streaming
    OPCODE_STREAMING_ON_OFF = 0x010F,
    // set specific mode: 0-normal; 1-fog
    OPCODE_SET_SPECIFIC_MODE = 0x0110,
    // set tail filter level
    OPCODE_SET_TAIL_FILTER_LEVEL = 0x0111,

    // set mems fov size
    OPCODE_SET_MEMS_FOV_SIZE = 0x0170,
    // set mems frenquency
    OPCODE_SET_MEMS_FRENQUENCY = 0x0171,
    // set mems fov factor
    OPCODE_SET_MEMS_FOV_FACTOR = 0x0172,
    // set repetitive scan mode
    OPCODE_SET_REPETITIVE_SCAN_MODE = 0x0173,
    // mems restart
    OPCODE_RESTART_MEMS = 0x0174,
    // save mems param
    OPCODE_SAVE_MEMS_PARAM = 0x0175,

    // scan mode get
    // get ip address
    OPCODE_GET_IP_ADDR = 0x0201,
    // get port
    OPCODE_GET_PORT = 0x0202,
    // get mac address
    OPCODE_GET_MAC_ADDR = 0x0203,
    // get subnet mask
    OPCODE_GET_SUBNET_MASK = 0x0204,
    // get scan speed
    OPCODE_GET_SCAN_SPEED = 0x0205,
    // get scan direction
    OPCODE_GET_SCAN_DIRECTION = 0x0206,
    // get transfer protocol
    OPCODE_GET_TRANSFER_PROTOCOL = 0x0207,
    // get work mode
    OPCODE_GET_WORK_MODE = 0x0208,
    // get device serial number
    OPCODE_GET_SERIAL_NUMBER = 0x0209,
    // get product model
    OPCODE_GET_PRODUCT_MODEL = 0x020A,
    // get firmware version
    OPCODE_GET_FIRMWARE_VERSION = 0x020B,
    // get FPGA version
    OPCODE_GET_FPGA_VERSION = 0x020C,
    // get status info
    OPCODE_GET_STATUS_INFO = 0x020D,
    // get warning info
    OPCODE_GET_WARNING_INFO = 0x020E,
    // get echo mode
    OPCODE_GET_ECHO_MODE = 0x020F,
    // get spin speed
    OPCODE_GET_MOTOR_SPIN_SPEED = 0x0210,
    // get MCU temperature
    OPCODE_GET_MCU_TEMPERATURE = 0x0211,
    // get FPGA temperature
    OPCODE_GET_FPGA_TEMPERATURE = 0x0212,
    // get motor version
    OPCODE_GET_MOTOR_VERSION = 0x0213,
    // get APD high voltage
    OPCODE_GET_APD_HIGH_VOLTAGE = 0x0214,
    // get APD temperature
    OPCODE_GET_APD_TEMPERATURE = 0x0215,
    // get TX high power voltage
    OPCODE_GET_TX_HIGH_POWER_VOLTAGE = 0x0216,
    // get specific mode
    OPCODE_GET_SPECIFIC_MODE = 0x0217,
    // get tail filter level
    OPCODE_GET_TAIL_FILTER_LEVEL = 0x0218,
    // get TX lower power voltage
    OPCODE_GET_TX_LOWER_POWER_VOLTAGE = 0x0219,

    // get mems fov size
    OPCODE_GET_MEMS_FOV_SIZE = 0x0270,
    // get mems frenquency
    OPCODE_GET_MEMS_FRENQUENCY = 0x0271,
    // get mems fov factor
    OPCODE_GET_MEMS_FOV_FACTOR = 0x0272,
    // get mems version
    OPCODE_GET_MEMS_VERSION = 0x0273,
    // get repetitive scan mode
    OPCODE_GET_REPETITIVE_SCAN_MODE = 0x0274,

    // calibration mode set TODO

    // calibration mode get TODO

    // unknown command
    OPCODE_UNKNOWN = 0xFFFF,
};

// Command operation hpStatus code -> used for program hpStatus return
enum HpStatusCode {
    HP_STATUS_OK = 0,

    // operation error
    HP_STATUS_ERROR_OPERATION_FAILURE,
    HP_STATUS_ERROR_TIMEOUT,
    HP_STATUS_ERROR_INVALID_REQ_ADDRESS,
    HP_STATUS_ERROR_OPERATION_UNSUPPORTED,

    HP_STATUS_CONTROL_TRANSFER_FAILED,  // Transmission error

    // Request error
    HP_STATUS_REQUEST_DATA_SIZE_ERROR,

    // Device response error
    HP_STATUS_DEVICE_RESPONSE_BAD_MAGIC,
    HP_STATUS_DEVICE_RESPONSE_WRONG_OPCODE,
    HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE,
    HP_STATUS_DEVICE_RESPONSE_BAD_CRC,
    HP_STATUS_DEVICE_RESPONSE_ERROR,

    HP_STATUS_UNKNOWN_ERROR = 0xffff,
};

// Status code returned by device command response -> used for communication between SDK and device
enum HpRespErrorCode {
    HP_RESP_OK = 0,

    // TODO single-line LiDAR error code is different from multi-lines lidar!!
    // error
    HP_RESP_ERROR_OPERATION_FAILURE     = 0x01,  // operation failure, such as invalid req data
    HP_RESP_ERROR_TIMEOUT               = 0x02,  // resp timeout
    HP_RESP_ERROR_INVALID_REQ_ADDRESS   = 0x03,  // invalid address
    HP_RESP_ERROR_OPERATION_UNSUPPORTED = 0x04,  // operation unsupported
    HP_RESP_ERROR_DEVICE_INNER          = 0x05,  // other error

    // unknown
    HP_RESP_ERROR_UNKNOWN = 0xff,
};

struct HpStatus {
    HpStatusCode    statusCode    = HP_STATUS_OK;
    HpRespErrorCode respErrorCode = HP_RESP_OK;
    std::string     msg;
};

inline std::ostream &operator<<(std::ostream &os, const HpStatus &s) {
    return (os << "HpStatusCode: " << s.statusCode << ", respErrorCode: " << s.respErrorCode << ", msg: " << s.msg);
}

#define HP_REQUEST_MAGIC 0x01FE
#define HP_RESPONSE_MAGIC 0x01FE

#define HP_NOT_READY_RETRIES 2

#define HP_PROTOCOL_VER_1 0x01

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;        // 0xfe01
    uint8_t  protocolVer;  // current version is 0x01
    uint16_t dataSize;     // data size
    uint16_t opCode;       // operation code
    uint16_t opInfo;       // for req, should be 0x0000
} ReqHeader;

typedef struct {
    uint16_t magic;        // 0xfe01
    uint8_t  protocolVer;  // current version is 0x01
    uint16_t dataSize;     // data size
    uint16_t opCode;       // operation code
    uint8_t  opDirection;  // operation direction, always 0x01
    uint8_t  opError;      // operation error
} RespHeader;

typedef struct {
    RespHeader header;
    int32_t    value;  // int32_t/uint32_t
} GetIntPropertyResp;

typedef struct {
    ReqHeader header;
    int32_t   value;  // int32_t/uint32_t
} SetIntPropertyReq;

typedef struct {
    RespHeader header;
    float      value;
} GetFloatPropertyResp;

typedef struct {
    ReqHeader header;
    float     value;
} SetFloatPropertyReq;

typedef struct {
    RespHeader header;
    uint8_t    data[1];
} GetRawDataResp;

#pragma pack(pop)

bool     checkStatus(HpStatus stat, bool throwException = true);
HpStatus execute(const std::shared_ptr<IVendorDataPort> &dataPort, uint16_t opCode, const uint8_t *reqData, uint16_t reqDataSize, uint8_t *respData,
                 uint16_t *respDataSize);

uint16_t initGetIntPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode);
uint16_t initSetIntPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode, uint32_t value);
uint16_t initGetFloatPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode);
uint16_t initSetFloatPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode, float value);
uint16_t initGetRawDataReq(std::vector<uint8_t> &dataBuf, uint16_t opCode);
uint16_t initSetRawDataReq(std::vector<uint8_t> &dataBuf, uint16_t opCode, const uint8_t *data, uint16_t dataSize);

GetIntPropertyResp   *parseGetIntPropertyResp(uint8_t *dataBuf, uint16_t dataSize);
GetFloatPropertyResp *parseGetFloatPropertyResp(uint8_t *dataBuf, uint16_t dataSize);
GetRawDataResp       *parseGetRawDataResp(uint8_t *dataBuf, uint16_t dataSize);
int16_t               getRaweDataSize(const GetRawDataResp *resp);

}  // namespace lidarprotocol
}  // namespace libobsensor
