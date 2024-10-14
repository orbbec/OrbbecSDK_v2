// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "NetDataStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "frame/FrameFactory.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

NetDataStreamPort::NetDataStreamPort(std::shared_ptr<const NetDataStreamPortInfo> portInfo) : portInfo_(portInfo), isStreaming_(false) {}

NetDataStreamPort::~NetDataStreamPort() noexcept{
    isStreaming_ = false;
    if(readDataThread_.joinable()) {
        readDataThread_.join();
    }

    if(tcpClient_) {
        tcpClient_.reset();
    }
}

void NetDataStreamPort::startStream(MutableFrameCallback callback) {
    if(isStreaming_) {
        throw wrong_api_call_sequence_exception("NetDataStreamPort::startStream() called when streaming");
    }
    callback_        = callback;
    auto netPortInfo = std::const_pointer_cast<NetDataStreamPortInfo>(portInfo_);
    tcpClient_       = std::make_shared<VendorTCPClient>(netPortInfo->address, netPortInfo->port);

    isStreaming_    = true;
    readDataThread_ = std::thread(&NetDataStreamPort::readData, this);
}

void NetDataStreamPort::stopStream() {
    if(!isStreaming_) {
        throw wrong_api_call_sequence_exception("NetDataStreamPort::stopStream() called when not streaming");
    }

    isStreaming_ = false;
    if(readDataThread_.joinable()) {
        readDataThread_.join();
    }

    if(tcpClient_) {
        tcpClient_.reset();
    }
}

void NetDataStreamPort::readData() {
    const int              PACK_SIZE     = 248;
    int                    dataRecvdSize = 0;
    int                    readSize      = 0;
    std::shared_ptr<Frame> frame;
    uint8_t               *data = nullptr;
    while(isStreaming_) {
        if(!frame) {
            frame         = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, PACK_SIZE);
            data          = frame->getDataMutable();
            dataRecvdSize = 0;
        }

        BEGIN_TRY_EXECUTE({ readSize = tcpClient_->read(data + dataRecvdSize, PACK_SIZE - dataRecvdSize); })
        CATCH_EXCEPTION_AND_EXECUTE({
            LOG_WARN("read data failed!");
            readSize = -1;
        })

        if(readSize < 0) {
            dataRecvdSize = 0;
        }
        else {
            dataRecvdSize += readSize;
        }

        if(PACK_SIZE == dataRecvdSize && isStreaming_) {
            auto realtime = utils::getNowTimesUs();
            frame->setSystemTimeStampUsec(realtime);
            callback_(frame);
            frame.reset();
        }
    }
}

}  // namespace libobsensor

