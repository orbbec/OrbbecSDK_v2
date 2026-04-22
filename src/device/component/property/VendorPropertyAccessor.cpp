// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VendorPropertyAccessor.hpp"
#include "exception/ObException.hpp"
#include "protocol/Protocol.hpp"
#include "property/InternalProperty.hpp"  // OB_PROP_DEVICE_RESET_BOOL

namespace libobsensor {

const uint32_t DEFAULT_CMD_MAX_DATA_SIZE  = 768;
const int      IO_ERROR_MAX_RETRIES       = 3;   // max retries on HP_STATUS_DEVICE_RESPONSE_IO_ERROR

VendorPropertyAccessor::VendorPropertyAccessor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend)
    : owner_(owner),
      backend_(backend),
      recvData_(1024),
      sendData_(1024),
      rawdataTransferPacketSize_(DEFAULT_CMD_MAX_DATA_SIZE),
      structListDataTransferPacketSize_(DEFAULT_CMD_MAX_DATA_SIZE) {
    auto port = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    if(!port) {
        THROW_INVALID_PARAM_EXCEPTION("VendorPropertyAccessor backend must be IVendorDataPort");
    }
}

void VendorPropertyAccessor::setRawdataTransferPacketSize(uint32_t size) {
    rawdataTransferPacketSize_ = size;
}

void VendorPropertyAccessor::setStructListDataTransferPacketSize(uint32_t size) {
    structListDataTransferPacketSize_ = size;
}

void VendorPropertyAccessor::setPropertyValue(uint32_t propertyId, const OBPropertyValue &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initSetPropertyReq(sendData_.data(), propertyId, value.intValue);

    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    uint16_t respDataSize = 0;
    executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);
}

void VendorPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetPropertyReq(sendData_.data(), propertyId);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

    auto resp       = protocol::parseGetPropertyResp(recvData_.data(), respDataSize);
    value->intValue = resp->data.cur;
}

void VendorPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetPropertyReq(sendData_.data(), propertyId);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

    auto resp            = protocol::parseGetPropertyResp(recvData_.data(), respDataSize);
    range->cur.intValue  = resp->data.cur;
    range->max.intValue  = resp->data.max;
    range->min.intValue  = resp->data.min;
    range->step.intValue = resp->data.step;
    range->def.intValue  = resp->data.def;
}

void VendorPropertyAccessor::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initSetStructureDataReq(sendData_.data(), propertyId, data.data(), static_cast<uint16_t>(data.size()));

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    uint16_t reqDataSize  = static_cast<uint16_t>(sizeof(*req) + data.size() - 1);
    executeAndCheck(port, sendData_.data(), reqDataSize, recvData_.data(), &respDataSize, propertyId);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureData(uint32_t propertyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetStructureDataReq(sendData_.data(), propertyId);

    uint16_t respDataSize    = 0;
    auto     port            = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    uint16_t expectedRespLen = (propertyId == OB_STRUCT_DEVICE_TIME) ? 64 : 0;  // TODO: optimized for GMSL device to reduce data transfer
    auto     res             = executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId, expectedRespLen);

    auto resp              = protocol::parseGetStructureDataResp(recvData_.data(), respDataSize);
    auto structureDataSize = protocol::getStructureDataSize(resp);
    if(structureDataSize <= 0) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_DATA_SIZE_ERROR;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data return data size invalid";
        protocol::checkStatus(propertyId, res);
    }
    outputData_.resize(structureDataSize);
    memcpy(outputData_.data(), resp->data, structureDataSize);
    return outputData_;
}

void VendorPropertyAccessor::getRawData(uint32_t propertyId, GetDataCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    OBDataTranState tranState = DATA_TRAN_STAT_TRANSFERRING;
    OBDataChunk     dataChunk = { sendData_.data(), 0, 0, 0 };
    uint32_t        dataSize;

    // init
    {
        clearBuffers();
        auto     req          = protocol::initGetRawDataLengthReq(sendData_.data(), propertyId, 0);
        uint16_t respDataSize = 64;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);
        auto resp = protocol::parseGetRawDataLengthResp(recvData_.data(), respDataSize);
        dataSize  = resp->dataSize;
    }

    // get raw data in packet size
    for(uint32_t packetOffset = 0; packetOffset < dataSize; packetOffset += rawdataTransferPacketSize_) {
        uint32_t packetLen = std::min(rawdataTransferPacketSize_, dataSize - packetOffset);
        clearBuffers();
        auto     req          = protocol::initReadRawDataReq(sendData_.data(), propertyId, packetOffset, packetLen);
        uint16_t respDataSize = 1024;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

        if(callback) {
            dataChunk.data         = recvData_.data() + sizeof(protocol::RespHeader);
            dataChunk.size         = packetLen;
            dataChunk.offset       = packetOffset;
            dataChunk.fullDataSize = dataSize;
            callback(tranState, &dataChunk);
        }
    }

    // finish
    {
        clearBuffers();
        auto     req          = protocol::initGetRawDataLengthReq(sendData_.data(), propertyId, 1);
        uint16_t respDataSize = 64;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);
    }
    dataChunk.data         = nullptr;
    dataChunk.size         = 0;
    dataChunk.offset       = dataSize;
    dataChunk.fullDataSize = dataSize;
    callback(DATA_TRAN_STAT_DONE, &dataChunk);
}

uint16_t VendorPropertyAccessor::getCmdVersionProtoV1_1(uint32_t propertyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto     req          = protocol::initGetCmdVersionReq(sendData_.data(), propertyId);
    uint16_t respDataSize = 64;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

    auto resp = protocol::parseGetCmdVerDataResp(recvData_.data(), respDataSize);
    return *(uint16_t *)(resp->data);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();

    auto     req          = protocol::initGetStructureDataReqV1_1(sendData_.data(), propertyId);
    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

    auto resp = protocol::parseGetStructureDataRespV1_1(recvData_.data(), respDataSize);
    if(resp->cmdVer != cmdVersion) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_CMD_VERSION_UNMATCHED;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data return cmd version unmatched: " + std::to_string(resp->cmdVer) + ", expect: " + std::to_string(cmdVersion);
        protocol::checkStatus(propertyId, res);
    }

    int16_t dataSize = protocol::getStructureDataSizeV1_1(resp);
    if(dataSize <= 0) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_DATA_SIZE_ERROR;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data over protocol version 1.1 return data size invalid";
        protocol::checkStatus(propertyId, res);
    }
    outputData_.resize(dataSize);
    memcpy(outputData_.data(), resp->data, dataSize);
    return outputData_;
}

void VendorPropertyAccessor::setStructureDataProtoV1_1(uint32_t propertyId, const std::vector<uint8_t> &data, uint16_t cmdVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto     req          = protocol::initSetStructureDataReqV1_1(sendData_.data(), propertyId, cmdVersion, data.data(), static_cast<uint16_t>(data.size()));
    uint16_t respDataSize = 64;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    uint16_t reqDataSize  = static_cast<uint16_t>(sizeof(*req) + data.size() - 1);
    executeAndCheck(port, sendData_.data(), reqDataSize, recvData_.data(), &respDataSize, propertyId);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t                    dataSize = 0;
    clearBuffers();
    auto     req          = protocol::initStartGetStructureDataListReq(sendData_.data(), propertyId);
    uint16_t respDataSize = 64;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = executeAndCheck(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize, propertyId);

    auto resp = protocol::parseStartStructureDataListResp(recvData_.data(), respDataSize);
    if(resp->cmdVer != cmdVersion) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_CMD_VERSION_UNMATCHED;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "init get structure data list return cmd version unmatched";
        protocol::checkStatus(propertyId, res);
    }
    dataSize = resp->dataSize;
    outputData_.resize(dataSize);
    {
        for(uint32_t packetOffset = 0; packetOffset < dataSize; packetOffset += structListDataTransferPacketSize_) {
            clearBuffers();  // reset request and response buffer cache
            uint32_t packetSize = std::min(structListDataTransferPacketSize_, dataSize - packetOffset);

            auto req1    = protocol::initGetStructureDataListReq(sendData_.data(), propertyId, packetOffset, packetSize);
            respDataSize = 1024;
            port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
            executeAndCheck(port, sendData_.data(), sizeof(*req1), recvData_.data(), &respDataSize, propertyId);
            memcpy(outputData_.data() + packetOffset, recvData_.data() + sizeof(protocol::RespHeader), packetSize);
        }
    }

    {
        clearBuffers();
        auto req2 = protocol::initFinishGetStructureDataListReq(sendData_.data(), propertyId);
        executeAndCheck(port, sendData_.data(), sizeof(*req2), recvData_.data(), &respDataSize, propertyId);
    }

    protocol::checkStatus(propertyId, res);
    return outputData_;
}

void VendorPropertyAccessor::setAutoRebootEnabled(bool enable) {
    autoRebootEnabled_ = enable;
}

void VendorPropertyAccessor::triggerReboot() {
    // Ensure reboot is triggered at most once per accessor instance.
    if(rebootTriggered_.exchange(true)) {
        return;
    }

    LOG_WARN("VendorPropertyAccessor: Protocol fault detected, triggering device reboot...");

    // Send OB_PROP_DEVICE_RESET_BOOL = 1 synchronously via backend_.
    // The caller is about to receive an exception, so blocking here is acceptable.
    // The response may be corrupted (same XU fault), but the send typically
    // succeeds and the device reboots; exceptions are swallowed.
    try {
        auto     port     = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        uint8_t  send[64] = {};
        uint8_t  recv[64] = {};
        uint16_t respSize = sizeof(recv);
        auto     req      = protocol::initSetPropertyReq(send, OB_PROP_DEVICE_RESET_BOOL, 1);
        auto     reqSize  = static_cast<uint16_t>(sizeof(*req));
        protocol::execute(port, send, reqSize, recv, &respSize);
    }
    catch(...) {
        // execute() threw, likely because the XU response was also corrupted.
        // The reset command may have been sent successfully; if so, the device
        // will go offline and the normal disconnect handler will deactivate it.
        LOG_WARN("VendorPropertyAccessor: reboot command threw, device may self-recover via disconnect.");
    }
}

protocol::HpStatus VendorPropertyAccessor::executeAndCheck(const std::shared_ptr<IVendorDataPort> &port,
                                                           uint8_t  *reqData,
                                                           uint16_t  reqDataSize,
                                                           uint8_t  *respData,
                                                           uint16_t *respDataSize,
                                                           uint32_t  propertyId,
                                                           uint16_t  expectedRespLen) {
    auto res = protocol::execute(port, reqData, reqDataSize, respData, respDataSize, expectedRespLen);

    // Only retry and trigger reboot when autoRebootEnabled_ is set (LibUVC path only).
    // V4L2 / WMF platforms never produce HP_STATUS_DEVICE_RESPONSE_IO_ERROR,
    // so this block is effectively dead on those platforms.
    if(autoRebootEnabled_ && res.statusCode == protocol::HP_STATUS_DEVICE_RESPONSE_IO_ERROR) {
        for(int retry = 0; retry < IO_ERROR_MAX_RETRIES; retry++) {
            LOG_WARN("VendorPropertyAccessor: response channel failed, retry {}/{}, propertyId: {}",
                     retry + 1, IO_ERROR_MAX_RETRIES, propertyId);
            res = protocol::execute(port, reqData, reqDataSize, respData, respDataSize, expectedRespLen);
            if(res.statusCode != protocol::HP_STATUS_DEVICE_RESPONSE_IO_ERROR) {
                break;
            }
        }

        if(res.statusCode == protocol::HP_STATUS_DEVICE_RESPONSE_IO_ERROR) {
            triggerReboot();
        }
    }

    protocol::checkStatus(propertyId, res);  // throws on error; caller behavior unchanged
    return res;
}

void VendorPropertyAccessor::clearBuffers() {
    memset(recvData_.data(), 0, recvData_.size());
    memset(sendData_.data(), 0, sendData_.size());
}

IDevice *VendorPropertyAccessor::getOwner() const {
    return owner_;
}

}  // namespace libobsensor
