// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UDPDataStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "frame/FrameFactory.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

UDPDataStreamPort::UDPDataStreamPort(std::shared_ptr<const NetDataStreamPortInfo> portInfo) : portInfo_(portInfo), isStreaming_(false) {}

UDPDataStreamPort::~UDPDataStreamPort() noexcept {
    isStreaming_ = false;
    if(readDataThread_.joinable()) {
        readDataThread_.join();
    }

    if(udpClient_) {
        udpClient_.reset();
    }
}

void UDPDataStreamPort::startStream(MutableFrameCallback callback) {
    if(isStreaming_) {
        throw wrong_api_call_sequence_exception("UDPDataStreamPort::startStream() called when streaming");
    }
    callback_        = callback;
    auto netPortInfo = std::const_pointer_cast<NetDataStreamPortInfo>(portInfo_);
    udpClient_       = std::make_shared<VendorUDPClient>(netPortInfo->address, netPortInfo->port);

    isStreaming_    = true;
    readDataThread_ = std::thread(&UDPDataStreamPort::readData, this);
}

void UDPDataStreamPort::stopStream() {
    if(!isStreaming_) {
        throw wrong_api_call_sequence_exception("UDPDataStreamPort::stopStream() called when not streaming");
    }

    isStreaming_ = false;
    if(readDataThread_.joinable()) {
        readDataThread_.join();
    }

    if(udpClient_) {
        udpClient_.reset();
    }
}

void UDPDataStreamPort::readData() {
    const int              PACK_SIZE = 1500;  // max size of UDP packet
    int                    readSize  = 0;
    uint8_t *              data      = nullptr;
    std::shared_ptr<Frame> frame;

    while(isStreaming_) {
        if(!frame) {
            frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, PACK_SIZE);
            data  = frame->getDataMutable();
        }

        BEGIN_TRY_EXECUTE({ readSize = udpClient_->read(data, PACK_SIZE); })
        CATCH_EXCEPTION_AND_EXECUTE({
            LOG_WARN("read data failed!");
            readSize = -1;
        })

        if(readSize > 0 && isStreaming_) {
            auto realtime = utils::getNowTimesUs();
            frame->setDataSize(readSize);
            frame->setSystemTimeStampUsec(realtime);
            callback_(frame);
            frame.reset();
        }
    }
}

}  // namespace libobsensor
