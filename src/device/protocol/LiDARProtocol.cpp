// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARProtocol.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace libobsensor {
namespace lidarprotocol {

uint8_t calcCrc8(const uint8_t *p, uint16_t len) {
    static const uint8_t crc8_table[256] = {
        0x00, 0x4d, 0x9a, 0xd7, 0x79, 0x34, 0xe3, 0xae, 0xf2, 0xbf, 0x68, 0x25, 0x8b, 0xc6, 0x11, 0x5c, 0xa9, 0xe4, 0x33, 0x7e, 0xd0, 0x9d, 0x4a, 0x07,
        0x5b, 0x16, 0xc1, 0x8c, 0x22, 0x6f, 0xb8, 0xf5, 0x1f, 0x52, 0x85, 0xc8, 0x66, 0x2b, 0xfc, 0xb1, 0xed, 0xa0, 0x77, 0x3a, 0x94, 0xd9, 0x0e, 0x43,
        0xb6, 0xfb, 0x2c, 0x61, 0xcf, 0x82, 0x55, 0x18, 0x44, 0x09, 0xde, 0x93, 0x3d, 0x70, 0xa7, 0xea, 0x3e, 0x73, 0xa4, 0xe9, 0x47, 0x0a, 0xdd, 0x90,
        0xcc, 0x81, 0x56, 0x1b, 0xb5, 0xf8, 0x2f, 0x62, 0x97, 0xda, 0x0d, 0x40, 0xee, 0xa3, 0x74, 0x39, 0x65, 0x28, 0xff, 0xb2, 0x1c, 0x51, 0x86, 0xcb,
        0x21, 0x6c, 0xbb, 0xf6, 0x58, 0x15, 0xc2, 0x8f, 0xd3, 0x9e, 0x49, 0x04, 0xaa, 0xe7, 0x30, 0x7d, 0x88, 0xc5, 0x12, 0x5f, 0xf1, 0xbc, 0x6b, 0x26,
        0x7a, 0x37, 0xe0, 0xad, 0x03, 0x4e, 0x99, 0xd4, 0x7c, 0x31, 0xe6, 0xab, 0x05, 0x48, 0x9f, 0xd2, 0x8e, 0xc3, 0x14, 0x59, 0xf7, 0xba, 0x6d, 0x20,
        0xd5, 0x98, 0x4f, 0x02, 0xac, 0xe1, 0x36, 0x7b, 0x27, 0x6a, 0xbd, 0xf0, 0x5e, 0x13, 0xc4, 0x89, 0x63, 0x2e, 0xf9, 0xb4, 0x1a, 0x57, 0x80, 0xcd,
        0x91, 0xdc, 0x0b, 0x46, 0xe8, 0xa5, 0x72, 0x3f, 0xca, 0x87, 0x50, 0x1d, 0xb3, 0xfe, 0x29, 0x64, 0x38, 0x75, 0xa2, 0xef, 0x41, 0x0c, 0xdb, 0x96,
        0x42, 0x0f, 0xd8, 0x95, 0x3b, 0x76, 0xa1, 0xec, 0xb0, 0xfd, 0x2a, 0x67, 0xc9, 0x84, 0x53, 0x1e, 0xeb, 0xa6, 0x71, 0x3c, 0x92, 0xdf, 0x08, 0x45,
        0x19, 0x54, 0x83, 0xce, 0x60, 0x2d, 0xfa, 0xb7, 0x5d, 0x10, 0xc7, 0x8a, 0x24, 0x69, 0xbe, 0xf3, 0xaf, 0xe2, 0x35, 0x78, 0xd6, 0x9b, 0x4c, 0x01,
        0xf4, 0xb9, 0x6e, 0x23, 0x8d, 0xc0, 0x17, 0x5a, 0x06, 0x4b, 0x9c, 0xd1, 0x7f, 0x32, 0xe5, 0xa8
    };

    uint8_t  crc = 0;
    uint16_t i;

    for(i = 0; i < len; i++) {
        crc = crc8_table[(crc ^ *p++) & 0xff];
    }
    return crc;
}

uint16_t getExpectedRespSize(uint16_t opCode) {
    (void)opCode;
    return 1024;  // TODO max size of response buffer, should be calculated based on opcode
}

/**
 * @brief validate response: RespHeader || data(0...n) || CRC8(1)
 */
HpStatus validateResp(uint8_t *dataBuf, uint16_t &dataSize, uint16_t expectedOpcode) {
    HpStatus    retStatus;
    RespHeader *header         = (RespHeader *)dataBuf;
    const auto  respHeaderSize = sizeof(RespHeader);

    if(dataSize <= respHeaderSize) {
        std::ostringstream ssMsg;
        ssMsg << "Device response size(" << dataSize << ") < response header size(" << respHeaderSize << ")";
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }
    if(header->magic != htons(HP_RESPONSE_MAGIC)) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with bad magic " << std::hex << ", magic=0x" << header->magic << ", expectOpCode=0x" << HP_RESPONSE_MAGIC;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_BAD_MAGIC;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    if(header->opCode != htons(expectedOpcode)) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with inconsistent opcode, opcode=" << header->opCode << ", expectedOpcode=" << expectedOpcode;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_OPCODE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }
    if ( header->opDirection != 0x01) {
        std::ostringstream ssMsg;
        ssMsg << "Device response with inconsistent opDirection, opDirection=" << header->opDirection << ", expectedOpcode=0x01";
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_ERROR;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    uint16_t respDataSize = ntohs(header->dataSize); // to host order
    if(respDataSize + respHeaderSize + 0x01 > dataSize) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = "Device response with wrong data size";
        return retStatus;
    }

    // check crc
    uint8_t crc = calcCrc8(dataBuf, respHeaderSize + respDataSize);
    if(crc != *(dataBuf + dataSize - 1)) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_BAD_CRC;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = "Device response data CRC check failed";
        return retStatus;
    }

    // to host order
    header->magic = ntohs(header->magic);
    header->opCode = ntohs(header->opCode);
    header->dataSize = ntohs(header->dataSize);

    switch(header->opError) {
    case HP_RESP_OK: {
        retStatus.statusCode = HP_STATUS_OK;
        retStatus.msg        = "";
        dataSize             = respHeaderSize + respDataSize;  // remove the crc byte from the end
        break;
    }
    case HP_RESP_ERROR_OPERATION_FAILURE: {
        retStatus.statusCode = HP_STATUS_ERROR_OPERATION_FAILURE;
        retStatus.msg        = "Operation failed, such as request data is invalid";
        break;
    }
    case HP_RESP_ERROR_TIMEOUT: {
        retStatus.statusCode = HP_STATUS_ERROR_TIMEOUT;
        retStatus.msg        = "Operation timed out";
        break;
    }
    case HP_RESP_ERROR_INVALID_REQ_ADDRESS: {
        retStatus.statusCode = HP_STATUS_ERROR_INVALID_REQ_ADDRESS;
        retStatus.msg        = "The specified register address is invalid";
        break;
    }
    case HP_RESP_ERROR_OPERATION_UNSUPPORTED: {
        retStatus.statusCode = HP_STATUS_ERROR_OPERATION_UNSUPPORTED;
        retStatus.msg        = "The Operation is unsupported";
        break;
    }
    default:
        retStatus.statusCode = HP_STATUS_UNKNOWN_ERROR;
        retStatus.msg        = "Device response with unknown error";
        break;
    }

    retStatus.respErrorCode = (HpRespErrorCode)header->opError;
    return retStatus;
}

bool checkStatus(HpStatus stat, bool throwException) {
    std::string retMsg;
    switch(stat.statusCode) {
    case HP_STATUS_OK:
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

HpStatus execute(const std::shared_ptr<IVendorDataPort> &dataPort, uint16_t opCode, const uint8_t *reqData, uint16_t reqDataSize, uint8_t *respData,
                 uint16_t *respDataSize) {
    HpStatusCode rc = HP_STATUS_OK;
    HpStatus     hpStatus;

    uint16_t nRetriesLeft = HP_NOT_READY_RETRIES;
    uint32_t exceptRecLen = getExpectedRespSize(opCode);

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
            hpStatus = validateResp(respData, *respDataSize, opCode);
            if(hpStatus.respErrorCode != HP_RESP_ERROR_DEVICE_INNER && hpStatus.respErrorCode != HP_RESP_ERROR_TIMEOUT) {
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

/**
 * @brief Check if the operation requires a parameter when set.
 */
bool operationRequiresParam(uint16_t opCode) {
    switch(opCode) {
    case OPCODE_REBOOT_DEVICE:
    case OPCODE_APPLY_CONFIGS:
    case OPCODE_RESTART_MEMS:
    case OPCODE_SAVE_MEMS_PARAM:
        return false;
    default:
        break;
    }
    return true;  // default is true
}

uint16_t initGetIntPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode) {
    const uint32_t headerSize = sizeof(ReqHeader);

    if(dataBuf.size() < headerSize + 1) {
        dataBuf.resize(headerSize + 1);
    }
    auto *   dataPtr = dataBuf.data();
    auto *   req     = reinterpret_cast<ReqHeader *>(dataPtr);
    uint8_t *crc     = dataPtr + headerSize;

    req->magic       = htons(HP_REQUEST_MAGIC);
    req->protocolVer = HP_PROTOCOL_VER_1;
    req->dataSize    = 0x0000;
    req->opCode      = htons(opCode);
    req->opInfo      = 0x0000;
    *crc             = calcCrc8(dataPtr, headerSize);

    return headerSize + 1;
}

uint16_t initSetIntPropertyReq(std::vector<uint8_t> &dataBuf, uint16_t opCode, uint32_t value) {
    const uint16_t headerSize = sizeof(ReqHeader);
    const uint16_t dataSize   = operationRequiresParam(opCode) ? 0x04 : 0;
    const uint16_t reqSize    = headerSize + dataSize + 1;  // add crc byte

    if(dataBuf.size() < reqSize) {
        dataBuf.resize(reqSize);
    }
    auto *   dataPtr = dataBuf.data();
    auto *   req     = reinterpret_cast<SetIntPropertyReq *>(dataPtr);
    uint8_t *crc     = dataPtr + headerSize + dataSize;

    req->header.magic       = htons(HP_REQUEST_MAGIC);
    req->header.protocolVer = HP_PROTOCOL_VER_1;
    req->header.dataSize    = htons(dataSize);
    req->header.opCode      = htons(opCode);
    req->header.opInfo      = 0x0000;
    req->value              = htonl(value);
    *crc                    = calcCrc8(dataPtr, headerSize + dataSize);

    return reqSize;
}

uint16_t initGetRawDataReq(std::vector<uint8_t> &dataBuf, uint16_t opCode) {
    const uint32_t headerSize = sizeof(ReqHeader);
    const uint16_t reqSize    = headerSize + 1;  // add crc byte

    if(dataBuf.size() < reqSize) {
        dataBuf.resize(reqSize);
    }
    auto *   dataPtr = dataBuf.data();
    auto *   req     = reinterpret_cast<ReqHeader *>(dataPtr);
    uint8_t *crc     = dataPtr + headerSize;

    req->magic       = htons(HP_REQUEST_MAGIC);
    req->protocolVer = HP_PROTOCOL_VER_1;
    req->dataSize    = 0x0000;
    req->opCode      = htons(opCode);
    req->opInfo      = 0x0000;
    *crc             = calcCrc8(dataPtr, headerSize);

    return reqSize;
}

uint16_t initSetRawDataReq(std::vector<uint8_t> &dataBuf, uint16_t opCode, const uint8_t *data, uint16_t dataSize) {
    const uint32_t headerSize = sizeof(ReqHeader);
    const uint16_t reqSize    = headerSize + dataSize + 1;  // add crc byte

    if(dataBuf.size() < reqSize) {
        dataBuf.resize(reqSize);
    }
    auto *   dataPtr = dataBuf.data();
    auto *   req     = reinterpret_cast<ReqHeader *>(dataPtr);
    uint8_t *crc     = dataPtr + headerSize + dataSize;

    // header
    req->magic       = htons(HP_REQUEST_MAGIC);
    req->protocolVer = HP_PROTOCOL_VER_1;
    req->dataSize    = htons(dataSize);
    req->opCode      = htons(opCode);
    req->opInfo      = 0x0000;
    // data
    memcpy(dataPtr + headerSize, data, dataSize);
    *crc = calcCrc8(dataPtr, headerSize + dataSize);

    return reqSize;
}

GetIntPropertyResp *parseGetIntPropertyResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetIntPropertyResp *>(dataBuf);
    if(dataSize < sizeof(GetIntPropertyResp) || resp->header.dataSize != sizeof(int32_t)) {
        throw io_exception("Device response with wrong data size");
    }
    resp->value = ntohl(resp->value); // to host order
    return resp;
}

GetRawDataResp *parseGetRawDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetRawDataResp *>(dataBuf);
    if(dataSize < sizeof(GetRawDataResp)) {
        throw io_exception("Device response with wrong data size");
    }
    return resp;
}

int16_t getRaweDataSize(const GetRawDataResp *resp) {
    return resp->header.dataSize;
}

}  // namespace lidarprotocol
}  // namespace libobsensor
