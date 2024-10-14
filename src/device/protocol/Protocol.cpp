// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Protocol.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include <sstream>

namespace libobsensor {
namespace protocol {

bool checkStatus(HpStatus stat, bool throwException) {
    std::string retMsg;
    switch(stat.statusCode) {
    case HP_STATUS_OK:
        break;
    case HP_STATUS_DEVICE_RESPONSE_WARNING:
        LOG_WARN("Request failed, device response with warning, errorCode: {0}, msg:{1}", stat.respErrorCode, stat.msg);
        return false;
    case HP_STATUS_DEVICE_RESPONSE_ERROR:
        retMsg = std::string("Request failed, device response with error, errorCode: ") + std::to_string(stat.respErrorCode) + ", msg: " + stat.msg;
        if(throwException) {
            throw io_exception(retMsg);
        }
        else {
            LOG_ERROR(retMsg);
            return false;
        }
        break;
    case HP_STATUS_DEVICE_RESPONSE_ERROR_UNKNOWN:
        if(throwException) {
            throw io_exception("Request failed, device response with unknown error!");
        }
        else {
            LOG_ERROR("Request failed, device response with unknown error!");
            return false;
        }
        break;

    default:
        retMsg = std::string("Request failed, statusCode: ") + std::to_string(stat.statusCode) + ", msg: " + stat.msg;
        if(throwException) {
            throw io_exception(retMsg);
        }
        else {
            LOG_ERROR(retMsg);
            return false;
        }
        break;
    }
    return true;
}

uint16_t getExpectedRespSize(HpOpCodes opcode) {
    uint16_t size = 64;
    if(opcode == OPCODE_GET_STRUCTURE_DATA || opcode == OPCODE_GET_STRUCTURE_DATA_V1_1 || opcode == OPCODE_HEARTBEAT_AND_STATE) {
        size = 512;
    }
    else if(opcode == OPCODE_READ_RAW_DATA || opcode == OPCODE_READ_STRUCT_DATA_LIST_V1_1) {
        size = 1024;
    }
    return size;
}

HpStatus validateResp(uint8_t *dataBuf, uint16_t dataSize, uint16_t expectedOpcode, uint16_t requestId) {
    HpStatus    retStatus;
    RespHeader *header = (RespHeader *)dataBuf;

    if(header->magic != HP_RESPONSE_MAGIC) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with bad magic " << std::hex << ", magic=0x" << header->magic << ", expectOpCode=0x" << HP_RESPONSE_MAGIC;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_BAD_MAGIC;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    if(header->requestId != requestId) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with inconsistent response requestId, cmdId=" << header->requestId << ", requestId=" << requestId;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_ID;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    if(header->opcode != expectedOpcode) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with inconsistent opcode, opcode=" << header->opcode << ", expectedOpcode=" << expectedOpcode;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_OPCODE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    uint16_t respDataSize = header->sizeInHalfWords * 2 - sizeof(RespHeader::errorCode);
    if(respDataSize + HP_RESP_HEADER_SIZE > dataSize) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = "Device response with wrong data size";
        return retStatus;
    }

    if(header->errorCode == HP_RESP_OK) {
        retStatus.statusCode    = HP_STATUS_OK;
        retStatus.respErrorCode = HP_RESP_OK;
        retStatus.msg           = "";
        return retStatus;
    }
    else if(header->errorCode == HP_RESP_ERROR_UNKNOWN) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_ERROR_UNKNOWN;
        retStatus.respErrorCode = (HpRespErrorCode)header->errorCode;
        retStatus.msg           = "Device response with unknown error";
        return retStatus;
    }

    std::string msg = respDataSize > 0 ? (char *)(dataBuf + sizeof(RespHeader)) : "";
    if(header->errorCode >= 0x8000 && header->errorCode <= 0xfffe) {
        retStatus.statusCode = HP_STATUS_DEVICE_RESPONSE_WARNING;
    }
    else {
        retStatus.statusCode = HP_STATUS_DEVICE_RESPONSE_ERROR;
    }
    retStatus.respErrorCode = (HpRespErrorCode)header->errorCode;
    retStatus.msg           = msg;
    return retStatus;
}

HpStatus execute(const std::shared_ptr<IVendorDataPort> &dataPort, uint8_t *reqData, uint16_t reqDataSize, uint8_t *respData, uint16_t *respDataSize) {
    HpStatusCode rc = HP_STATUS_OK;
    HpStatus     hpStatus;

    uint16_t requestId    = ((ReqHeader *)(reqData))->requestId;
    uint16_t opcode       = ((ReqHeader *)(reqData))->opcode;
    uint16_t nRetriesLeft = HP_NOT_READY_RETRIES;
    uint32_t exceptRecLen = getExpectedRespSize(static_cast<HpOpCodes>(opcode));

    while(nRetriesLeft-- > 0)  // loop until device is ready
    {
        rc = HP_STATUS_OK;
        try {
            *respDataSize = static_cast<uint16_t>(dataPort->sendAndReceive(reqData, static_cast<uint32_t>(reqDataSize), respData, exceptRecLen));
        }
        catch(...) {
            rc = HP_STATUS_CONTROL_TRANSFER_FAILED;
        }

        if(rc == HP_STATUS_OK) {
            hpStatus = validateResp(respData, *respDataSize, opcode, requestId);

            if(hpStatus.statusCode != HP_STATUS_DEVICE_RESPONSE_WRONG_ID && hpStatus.respErrorCode != HP_RESP_ERROR_DEVICE_BUSY) {
                break;
            }
        }
        else {
            hpStatus.statusCode    = rc;
            hpStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
            hpStatus.msg           = "Send control transfer failed!";
            break;
        }

        // Retry after delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return hpStatus;
}

uint16_t generateRequestId() {
    static uint16_t requestId = 0;
    requestId++;
    return requestId;
}

GetPropertyReq *initGetPropertyReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_PROPERTY;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    return req;
}

SetPropertyReq *initSetPropertyReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t value) {
    auto *req                   = reinterpret_cast<SetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 4;
    req->header.opcode          = OPCODE_SET_PROPERTY;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    req->value                  = value;
    return req;
}

GetStructureDataReq *initGetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetStructureDataReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_STRUCTURE_DATA;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

SetStructureDataReq *initSetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId, const uint8_t *data, uint16_t dataSize) {
    auto *req                   = reinterpret_cast<SetStructureDataReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2 + (dataSize + 1) / 2;
    req->header.opcode          = OPCODE_SET_STRUCTURE_DATA;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    memcpy(req->data, data, dataSize);
    return req;
}

HeartbeatAndStateReq *initHeartbeatAndStateReq(uint8_t *dataBuf) {
    auto *req                   = reinterpret_cast<HeartbeatAndStateReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 0;
    req->header.opcode          = OPCODE_HEARTBEAT_AND_STATE;
    req->header.requestId       = generateRequestId();
    return req;
}

HeartbeatAndStateResp *parseHeartbeatAndStateResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<HeartbeatAndStateResp *>(dataBuf);
    if(dataSize < sizeof(HeartbeatAndStateResp) - 1) {  // may dose'nt have any msg, subtract 1 byte to avoid overflow
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

GetPropertyResp *parseGetPropertyResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetPropertyResp *>(dataBuf);
    if(dataSize < sizeof(GetPropertyResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

SetPropertyResp *parseSetPropertyResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<SetPropertyResp *>(dataBuf);
    if(dataSize < sizeof(SetPropertyResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

GetStructureDataResp *parseGetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetStructureDataResp *>(dataBuf);
    if(dataSize < sizeof(GetStructureDataResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

int16_t getStructureDataSize(const GetStructureDataResp *resp) {
    return resp->header.sizeInHalfWords * 2 - sizeof(RespHeader::errorCode);
}

SetStructureDataResp *parseSetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<SetStructureDataResp *>(dataBuf);
    if(dataSize < sizeof(SetStructureDataResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

GetPropertyReq *initGetCmdVersionReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_COMMAND_VERSION_V1_1;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    return req;
}

GetCmdVerDataResp *parseGetCmdVerDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetCmdVerDataResp *>(dataBuf);
    if(dataSize < sizeof(GetCmdVerDataResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

GetRawDataLengthResp *parseGetRawDataLengthResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetRawDataLengthResp *>(dataBuf);
    if(dataSize < sizeof(GetRawDataLengthResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

ReadRawDataResp *parseReadRawDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<ReadRawDataResp *>(dataBuf);
    if(dataSize < sizeof(ReadRawDataResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

GetStructureDataReqV1_1 *initGetStructureDataReqV1_1(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetStructureDataReqV1_1 *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_STRUCTURE_DATA_V1_1;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

SetStructureDataReqV1_1 *initSetStructureDataReqV1_1(uint8_t *dataBuf, uint32_t propertyId, uint16_t cmdVer, const uint8_t *data, uint16_t dataSize) {
    auto *req                   = reinterpret_cast<SetStructureDataReqV1_1 *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 3 + (dataSize + 1) / 2;
    req->header.opcode          = OPCODE_SET_STRUCTURE_DATA_V1_1;
    req->header.requestId       = generateRequestId();
    req->cmdVer                 = cmdVer;
    req->propertyId             = propertyId;
    memcpy(req->data, data, dataSize);
    return req;
}

GetStructureDataRespV1_1 *parseGetStructureDataRespV1_1(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetStructureDataRespV1_1 *>(dataBuf);
    if(dataSize < sizeof(GetStructureDataRespV1_1)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

SetStructureDataRespV1_1 *parseSetStructureDataRespV1_1(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<SetStructureDataRespV1_1 *>(dataBuf);
    if(dataSize < sizeof(SetStructureDataRespV1_1)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

StartGetStructureDataListResp *parseStartStructureDataListResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<StartGetStructureDataListResp *>(dataBuf);
    if(dataSize < sizeof(StartGetStructureDataListResp)) {
        throw io_exception("Device response with wrong data size");
    }

    return resp;
}

int16_t getStructureDataSizeV1_1(const GetStructureDataRespV1_1 *resp) {
    return resp->header.sizeInHalfWords * 2 - sizeof(RespHeader::errorCode) - 2;
}

StartGetStructureDataListReq *initStartGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<StartGetStructureDataListReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_INIT_READ_STRUCT_DATA_LIST_V1_1;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

GetStructureDataListReq *initGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t dataSize) {
    auto *req                   = reinterpret_cast<GetStructureDataListReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 6;
    req->header.opcode          = OPCODE_READ_STRUCT_DATA_LIST_V1_1;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;

    req->offset = offset;
    req->size   = dataSize;
    return req;
}

FinishGetStructureDataListReq *initFinishGetStructureDataListReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<FinishGetStructureDataListReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_FINISH_READ_STRUCT_DATA_LIST_V1_1;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

GetPropertyReq *initGetRawDataLengthReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t cmd) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    switch(cmd) {
    case 0:
        req->header.opcode = OPCODE_INIT_READ_RAW_DATA;
        break;
    case 1:
        req->header.opcode = OPCODE_FINISH_READ_RAW_DATA;
        break;
    }

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

ReadRawDataReq *initReadRawDataReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t size) {
    auto *req                   = reinterpret_cast<ReadRawDataReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 6;
    req->header.opcode          = OPCODE_READ_RAW_DATA;
    req->header.requestId       = generateRequestId();

    req->propertyId = propertyId;
    req->offset     = offset;
    req->size       = size;
    return req;
}

}  // namespace protocol
}  // namespace libobsensor

