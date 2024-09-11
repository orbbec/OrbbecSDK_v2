#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

RTPStreamPort::RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo) : portInfo_(portInfo), streamStarted_(false) {}

RTPStreamPort::~RTPStreamPort() {
    TRY_EXECUTE(stopAllStream());
}

StreamProfileList RTPStreamPort::getStreamProfileList() {
    return streamProfileList_;
}

void RTPStreamPort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(!streamStarted_) {
        BEGIN_TRY_EXECUTE({ startClientTask(profile, callback); })
        CATCH_EXCEPTION_AND_EXECUTE({
            closeClientTask();
            throw;
        })
        streamStarted_ = true;
        LOG_DEBUG("{} Stream started!", utils::obStreamToStr(profile->getType()));
    }
    else {
        LOG_WARN("{} stream already started!", utils::obStreamToStr(profile->getType()));
    }
}

void RTPStreamPort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    stopAllStream();
}

void RTPStreamPort::stopAllStream() {
    closeClientTask();
    streamStarted_ = false;
}

void RTPStreamPort::startClientTask(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(rtpClient_) {
        rtpClient_.reset();
    }
    rtpClient_ = std::make_shared<ObRTPClient>();
    rtpClient_->start(portInfo_->localAddress, portInfo_->address, portInfo_->port, profile, callback);
}

void RTPStreamPort::closeClientTask() {
    if(rtpClient_) {
        rtpClient_->close();
        rtpClient_.reset();
    }
    LOG_DEBUG("Stream stop!");
}

void RTPStreamPort::startStream(MutableFrameCallback callback) {
    if(!streamStarted_) {
        BEGIN_TRY_EXECUTE({ startClientTask(nullptr, callback); })
        CATCH_EXCEPTION_AND_EXECUTE({
            closeClientTask();
            throw;
        })
        streamStarted_ = true;
        LOG_DEBUG("IMU Stream started!");
    }
    else {
        LOG_WARN("IMU stream already started!");
    }
}

void RTPStreamPort::stopStream() {
    stopAllStream();
}

std::shared_ptr<const SourcePortInfo> RTPStreamPort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor