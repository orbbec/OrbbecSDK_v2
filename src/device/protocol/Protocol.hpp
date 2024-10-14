// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include <sstream>

namespace libobsensor {
namespace protocol {

// HP mean hardware protocol

#define HP_REQUEST_MAGIC 0x4d47   // MG
#define HP_RESPONSE_MAGIC 0x4252  // BR

#define HP_REQ_HEADER_SIZE sizeof(ReqHeader)
#define HP_RESP_HEADER_SIZE sizeof(RespHeader)

#define HP_NOT_READY_RETRIES 2

enum HpOpCodes {
    OPCODE_GET_PROPERTY        = 1,
    OPCODE_SET_PROPERTY        = 2,
    OPCODE_GET_STRUCTURE_DATA  = 3,
    OPCODE_SET_STRUCTURE_DATA  = 4,
    OPCODE_HEARTBEAT_AND_STATE = 5,

    OPCODE_INIT_READ_RAW_DATA   = 17,
    OPCODE_READ_RAW_DATA        = 18,
    OPCODE_FINISH_READ_RAW_DATA = 19,

    // v1.1 protocol introduction
    OPCODE_GET_COMMAND_VERSION_V1_1 = 26,  // v1.1 control command version number, determines how the control command parses data content
    OPCODE_GET_STRUCTURE_DATA_V1_1  = 27,  // v1.1 structure type data reading
    OPCODE_SET_STRUCTURE_DATA_V1_1  = 28,  // v1.1 Structure type data setting

    OPCODE_INIT_READ_STRUCT_DATA_LIST_V1_1    = 29,    // v1.1 Initialize structure type list data reading
    OPCODE_READ_STRUCT_DATA_LIST_V1_1         = 30,    // v1.1 Structure type list data reading
    OPCODE_FINISH_READ_STRUCT_DATA_LIST_V1_1  = 31,    // v1.1 ends structure type list data reading
    OPCODE_INIT_WRITE_STRUCT_DATA_LIST_V1_1   = 32,    // v1.1 Initialize structure type list data writing
    OPCODE_WRITE_STRUCT_DATA_LIST_V1_1        = 33,    // v1.1 Structure type list data writing
    OPCODE_FINISH_WRITE_STRUCT_DATA_LIST_V1_1 = 34,    // v1.1 ends writing structure type list data
    OPCODE_GET_PROTOCOL_VERSION               = 0xfe,  // Get the protocol version number supported by the firmware
};

// Command operation hpStatus code -> used for program hpStatus return
enum HpStatusCode {
    HP_STATUS_OK = 0,

    HP_STATUS_NO_DEVICE_FOUND,
    HP_STATUS_CONTROL_TRANSFER_FAILED,  // Transmission error
    HP_STATUS_DATA_TRANSFER_BUSY,       // Data transfer busy

    // Request error
    HP_STATUS_REQUEST_DATA_SIZE_ERROR,

    // Device response error
    HP_STATUS_DEVICE_RESPONSE_BAD_MAGIC,
    HP_STATUS_DEVICE_RESPONSE_WRONG_ID,
    HP_STATUS_DEVICE_RESPONSE_WRONG_OPCODE,
    HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE,
    HP_STATUS_DEVICE_RESPONSE_ERROR,            // error code=0x0001~0x7fff
    HP_STATUS_DEVICE_RESPONSE_WARNING,          // error code=0x8000~0xfffe
    HP_STATUS_DEVICE_RESPONSE_ERROR_UNKNOWN,    // error code = 0xffff
    HP_STATUS_DEVICE_RESPONSE_DATA_SIZE_ERROR,  //
    HP_STATUS_DEVICE_RESPONSE_CMD_VERSION_UNMATCHED,
    HP_STATUS_UNKNOWN_ERROR = 0xffff,
};

// Status code returned by device command response -> used for communication between SDK and device
enum HpRespErrorCode {
    HP_RESP_OK = 0,

    // error
    HP_RESP_ERROR_INVALID_REQUEST     = 0x0001,
    HP_RESP_ERROR_UNSUPPORTED_REQUEST = 0x0002,
    HP_RESP_ERROR_DEVICE_BUSY         = 0x0003,
    HP_RESP_ERROR_ACCESS_FORBIDDEN    = 0x0004,

    // warning
    HP_RESP_WARNING_REPETITIVE_REQUEST = 0x8000,

    // unknown
    HP_RESP_ERROR_UNKNOWN = 0xffff,
};

struct HpStatus {
    HpStatusCode    statusCode    = HP_STATUS_OK;
    HpRespErrorCode respErrorCode = HP_RESP_OK;
    std::string     msg;
};

inline std::ostream &operator<<(std::ostream &os, const HpStatus &s) {
    return (os << "HpStatusCode: " << s.statusCode << ", respErrorCode: " << s.respErrorCode << ", msg: " << s.msg);
}

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint16_t sizeInHalfWords;  // bytes/2
    uint16_t opcode;
    uint16_t requestId;
} ReqHeader;

typedef struct {
    uint16_t magic;
    uint16_t sizeInHalfWords;  // bytes/2
    uint16_t opcode;
    uint16_t requestId;  // device response requestId same as request requestId, used for matching request and response
    uint16_t errorCode;
} RespHeader;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
} GetPropertyReq;

typedef struct {
    int32_t cur;  // int32_t means 4 bytes data, type is depends on propertyId
    int32_t max;
    int32_t min;
    int32_t def;
    int32_t step;
} PropertyData;

typedef struct {
    RespHeader   header;
    PropertyData data;
} GetPropertyResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
    int32_t   value;  // int32_t means 4 bytes data, type is depends on propertyId
} SetPropertyReq;

typedef struct {
    RespHeader header;
} SetPropertyResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
} GetStructureDataReq;

typedef struct {
    RespHeader header;
    uint8_t    data[1];
} GetStructureDataResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
    uint8_t   data[1];
} SetStructureDataReq;

typedef struct {
    RespHeader header;
} SetStructureDataResp;

typedef struct {
    ReqHeader header;
    uint8_t   data[1];
} GetCmdVerDataResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
    uint16_t  cmdVer;
    uint8_t   data[1];
} SetStructureDataReqV1_1;

typedef struct {
    RespHeader header;
} SetStructureDataRespV1_1;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
} GetStructureDataReqV1_1;

typedef struct {
    RespHeader header;
    uint16_t   cmdVer;
    uint8_t    data[1];
} GetStructureDataRespV1_1;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
} StartGetStructureDataListReq;

typedef struct {
    RespHeader header;
    uint16_t   cmdVer;
    uint32_t   dataSize;
} StartGetStructureDataListResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
    uint32_t  offset;
    uint32_t  size;
} GetStructureDataListReq;

typedef struct {
    RespHeader header;
    uint16_t   cmdVer;
    uint32_t   dataSize;
} GetStructureDataListResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
} FinishGetStructureDataListReq;

typedef struct {
    RespHeader header;
} FinishGetStructureDataListResp;

typedef struct {
    ReqHeader header;
    uint32_t  propertyId;
    uint32_t  offset;
    uint32_t  size;
} ReadRawDataReq;

typedef struct {
    RespHeader header;
    uint8_t    data[1];
} ReadRawDataResp;

typedef struct {
    RespHeader header;
    uint32_t   dataSize;
} GetRawDataLengthResp;

typedef struct {
    ReqHeader header;
} HeartbeatAndStateReq;

typedef struct {
    RespHeader header;
    uint64_t   state;
    char       message[1];
} HeartbeatAndStateResp;

#pragma pack(pop)

GetPropertyReq      *initGetPropertyReq(uint8_t *dataBuf, uint32_t propertyId);
SetPropertyReq      *initSetPropertyReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t value);
GetStructureDataReq *initGetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId);
SetStructureDataReq *initSetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId, const uint8_t *data, uint16_t dataSize);
GetPropertyReq      *initGetCmdVersionReq(uint8_t *dataBuf, uint32_t propertyId);

GetStructureDataReqV1_1 *initGetStructureDataReqV1_1(uint8_t *dataBuf, uint32_t propertyId);
SetStructureDataReqV1_1 *initSetStructureDataReqV1_1(uint8_t *dataBuf, uint32_t propertyId, uint16_t cmdVer, const uint8_t *data, uint16_t dataSize);

StartGetStructureDataListReq  *initStartGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId);
GetStructureDataListReq       *initGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t dataSize);
FinishGetStructureDataListReq *initFinishGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId);
GetPropertyReq                *initGetRawDataLengthReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t cmd);
ReadRawDataReq                *initReadRawDataReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t size);

HeartbeatAndStateReq *initHeartbeatAndStateReq(uint8_t *dataBuf);

GetPropertyResp               *parseGetPropertyResp(uint8_t *dataBuf, uint16_t dataSize);
SetPropertyResp               *parseSetPropertyResp(uint8_t *dataBuf, uint16_t dataSize);
GetStructureDataResp          *parseGetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize);
int16_t                        getStructureDataSize(const GetStructureDataResp *resp);
GetStructureDataRespV1_1      *parseGetStructureDataRespV1_1(uint8_t *dataBuf, uint16_t dataSize);
SetStructureDataRespV1_1      *parseSetStructureDataRespV1_1(uint8_t *dataBuf, uint16_t dataSize);
int16_t                        getStructureDataSizeV1_1(const GetStructureDataRespV1_1 *resp);
SetStructureDataResp          *parseSetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize);
GetCmdVerDataResp             *parseGetCmdVerDataResp(uint8_t *dataBuf, uint16_t dataSize);
GetRawDataLengthResp          *parseGetRawDataLengthResp(uint8_t *dataBuf, uint16_t dataSize);
ReadRawDataResp               *parseReadRawDataResp(uint8_t *dataBuf, uint16_t dataSize);
StartGetStructureDataListResp *parseStartStructureDataListResp(uint8_t *dataBuf, uint16_t dataSize);
HeartbeatAndStateResp         *parseHeartbeatAndStateResp(uint8_t *dataBuf, uint16_t dataSize);

HpStatus execute(const std::shared_ptr<IVendorDataPort> &dataPort, uint8_t *reqData, uint16_t reqDataSize, uint8_t *respData, uint16_t *respDataSize);
bool     checkStatus(HpStatus stat, bool throwException = true);

}  // namespace protocol
}  // namespace libobsensor

