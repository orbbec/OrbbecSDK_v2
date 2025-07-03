// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDataStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "frame/FrameFactory.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

LiDARDataStreamPort::LiDARDataStreamPort(std::shared_ptr<const LiDARDataStreamPortInfo> portInfo) : portInfo_(portInfo), isStreaming_(false), callback_(nullptr) {}

LiDARDataStreamPort::~LiDARDataStreamPort() noexcept {
    stop();
}

void LiDARDataStreamPort::startStream(MutableFrameCallback callback) {
    if(isStreaming_) {
        throw wrong_api_call_sequence_exception("UDPDataStreamPort::startStream() called when streaming");
    }
    callback_        = callback;
    auto netPortInfo = std::const_pointer_cast<LiDARDataStreamPortInfo>(portInfo_);
    udpClient_       = std::make_shared<VendorUDPClient>(netPortInfo->address, netPortInfo->port, 500);

    // connect and tell the port to the device
    // TODO: should use the LiDARProtocol API
    udpClient_->write((const uint8_t*)"\x01\xfe\x01\x00\x04\x01\x09\x00\x00\x12\x34\x56\x78\x84", 14);
    uint8_t response[0x100];
    int ret = udpClient_->read(response, sizeof(response));
    if(ret != 10 || 0 != memcmp(response, "\x01\xFE\x01\x00\x00\x01\x09\x01\x00\x1F", ret) || response[8] != 0x00) {
        // reset the udpClient_
        udpClient_.reset();
        throw io_exception("UDPDataStreamPort::startStream() failed to connect device");
    }
    isStreaming_    = true;
    readDataThread_ = std::thread(&LiDARDataStreamPort::readData, this);
}

void LiDARDataStreamPort::stopStream() {

    // catch exceptions, don't throw them.
    BEGIN_TRY_EXECUTE({
        if(!isStreaming_.load()) {
            // throw an exception may result in stop() not being called
            LOG_WARN("UDPDataStreamPort::stopStream() called when not streaming");
        }

        stop();
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("stop stream failed"); })
}

void LiDARDataStreamPort::stop() {
    isStreaming_.store(false);
    if(readDataThread_.joinable()) {
        readDataThread_.join();
    }
    if(udpClient_) {
        udpClient_.reset();
    }
}

void LiDARDataStreamPort::readData() {
    const int              PACK_SIZE = 1500;  // max size of UDP packet
    int                    readSize  = 0;
    uint8_t *              data      = nullptr;
    std::shared_ptr<Frame> frame;

    while(isStreaming_.load()) {
        if(!frame) {
            frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, PACK_SIZE);
            data  = frame->getDataMutable();
        }

        BEGIN_TRY_EXECUTE({ readSize = udpClient_->read(data, PACK_SIZE); })
        CATCH_EXCEPTION_AND_EXECUTE({
            LOG_WARN("read data failed!");
            readSize = -1;
        })

        if(readSize > 0 && isStreaming_.load()) {
            auto realtime = utils::getNowTimesUs();
            frame->setDataSize(readSize);
            frame->setSystemTimeStampUsec(realtime);
            callback_(frame);
            frame.reset();
        }
    }
    LOG_DEBUG("LiDARDataStreamPort: quit read data");
}

}  // namespace libobsensor
