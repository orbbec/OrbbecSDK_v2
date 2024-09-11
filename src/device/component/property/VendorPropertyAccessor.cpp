#include "VendorPropertyAccessor.hpp"
#include "exception/ObException.hpp"
#include "protocol/Protocol.hpp"

namespace libobsensor {

const uint32_t DEFAULT_CMD_MAX_DATA_SIZE = 768;

VendorPropertyAccessor::VendorPropertyAccessor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend)
    : owner_(owner),
      backend_(backend),
      recvData_(1024),
      sendData_(1024),
      rawdataTransferPacketSize_(DEFAULT_CMD_MAX_DATA_SIZE),
      structListDataTransferPacketSize_(DEFAULT_CMD_MAX_DATA_SIZE) {
    auto port = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    if(!port) {
        throw invalid_value_exception("VendorPropertyAccessor backend must be IVendorDataPort");
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
    auto     res = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);  // todo: check respDataSize, here and below
    protocol::checkStatus(res);
}

void VendorPropertyAccessor::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetPropertyReq(sendData_.data(), propertyId);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);

    protocol::checkStatus(res);

    auto resp       = protocol::parseGetPropertyResp(recvData_.data(), respDataSize);
    value->intValue = resp->data.cur;
}

void VendorPropertyAccessor::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetPropertyReq(sendData_.data(), propertyId);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
    protocol::checkStatus(res);

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
    auto     res          = protocol::execute(port, sendData_.data(), reqDataSize, recvData_.data(), &respDataSize);
    protocol::checkStatus(res);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureData(uint32_t propertyId) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();
    auto req = protocol::initGetStructureDataReq(sendData_.data(), propertyId);

    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
    protocol::checkStatus(res);

    auto resp              = protocol::parseGetStructureDataResp(recvData_.data(), respDataSize);
    auto structureDataSize = protocol::getStructureDataSize(resp);
    if(structureDataSize <= 0) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_DATA_SIZE_ERROR;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data return data size invalid";
        protocol::checkStatus(res);
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
        auto     req          = protocol::initGetRawDataLength(sendData_.data(), propertyId, 0);
        uint16_t respDataSize = 64;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
        protocol::checkStatus(res);
        auto resp = protocol::parseGetRawDataLengthResp(recvData_.data(), respDataSize);
        dataSize  = resp->dataSize;
    }

    // get raw data in packet size
    for(uint32_t packetOffset = 0; packetOffset < dataSize; packetOffset += rawdataTransferPacketSize_) {
        uint32_t packetLen = std::min(rawdataTransferPacketSize_, dataSize - packetOffset);
        clearBuffers();
        auto     req          = protocol::initReadRawData(sendData_.data(), propertyId, packetOffset, packetLen);
        uint16_t respDataSize = 1024;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
        protocol::checkStatus(res);

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
        auto     req          = protocol::initGetRawDataLength(sendData_.data(), propertyId, 1);
        uint16_t respDataSize = 64;
        auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
        auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
        protocol::checkStatus(res);
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
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
    protocol::checkStatus(res);

    auto resp = protocol::parseGetCmdVerDataResp(recvData_.data(), respDataSize);
    return *(uint16_t *)(resp->data);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureDataProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    clearBuffers();

    auto     req          = protocol::initGetStructureDataReqV1_1(sendData_.data(), propertyId);
    uint16_t respDataSize = 0;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
    protocol::checkStatus(res);

    auto resp = protocol::parseGetStructureDataRespV1_1(recvData_.data(), respDataSize);
    if(resp->cmdVer != cmdVersion) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_CMD_VERSION_UNMATCHED;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data return cmd version unmatched: " + std::to_string(resp->cmdVer) + ", expect: " + std::to_string(cmdVersion);
        protocol::checkStatus(res);
    }

    int16_t dataSize = protocol::getStructureDataSizeV1_1(resp);
    if(dataSize <= 0) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_DATA_SIZE_ERROR;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "get structure data over protocol version 1.1 return data size invalid";
        protocol::checkStatus(res);
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
    auto     res          = protocol::execute(port, sendData_.data(), reqDataSize, recvData_.data(), &respDataSize);
    protocol::checkStatus(res);
}

const std::vector<uint8_t> &VendorPropertyAccessor::getStructureDataListProtoV1_1(uint32_t propertyId, uint16_t cmdVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t                    dataSize = 0;
    clearBuffers();
    auto     req          = protocol::initStartGetStructureDataList(sendData_.data(), propertyId);
    uint16_t respDataSize = 64;
    auto     port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
    auto     res          = protocol::execute(port, sendData_.data(), sizeof(*req), recvData_.data(), &respDataSize);
    protocol::checkStatus(res);

    auto resp = protocol::parseStartStructureDataListResp(recvData_.data(), respDataSize);
    if(resp->cmdVer != cmdVersion) {
        res.statusCode    = protocol::HP_STATUS_DEVICE_RESPONSE_CMD_VERSION_UNMATCHED;
        res.respErrorCode = protocol::HP_RESP_ERROR_UNKNOWN;
        res.msg           = "init get structure data list return cmd version unmatched";
        protocol::checkStatus(res);
    }
    dataSize = resp->dataSize;
    outputData_.resize(dataSize);
    {
        for(uint32_t packetOffset = 0; packetOffset < dataSize; packetOffset += structListDataTransferPacketSize_) {
            clearBuffers();  // reset request and response buffer cache
            uint32_t packetSize = std::min(structListDataTransferPacketSize_, dataSize - packetOffset);

            auto req1    = protocol::initGetStructureDataList(sendData_.data(), propertyId, packetOffset, packetSize);
            respDataSize = 1024;
            port         = std::dynamic_pointer_cast<IVendorDataPort>(backend_);
            res          = protocol::execute(port, sendData_.data(), sizeof(*req1), recvData_.data(), &respDataSize);
            protocol::checkStatus(res);
            memcpy(outputData_.data() + packetOffset, recvData_.data() + sizeof(protocol::RespHeader), packetSize);
        }
    }

    {
        clearBuffers();
        auto req2 = protocol::initFinishGetStructureDataList(sendData_.data(), propertyId);
        res       = protocol::execute(port, sendData_.data(), sizeof(*req2), recvData_.data(), &respDataSize);
        protocol::checkStatus(res);
    }

    protocol::checkStatus(res);
    return outputData_;
}

void VendorPropertyAccessor::clearBuffers() {
    memset(recvData_.data(), 0, recvData_.size());
    memset(sendData_.data(), 0, sendData_.size());
}

IDevice *VendorPropertyAccessor::getOwner() const {
    return owner_;
}

}  // namespace libobsensor