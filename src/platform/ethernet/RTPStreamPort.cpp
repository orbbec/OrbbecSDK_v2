#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

RTPStreamPort::RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo) : portInfo_(portInfo), streamStarted_(false) {
    initRTPClient();
}

RTPStreamPort::~RTPStreamPort() {
    TRY_EXECUTE(stopAllStream());
}

void RTPStreamPort::initRTPClient() {
    if(rtpClient_) {
        rtpClient_.reset();
    }
    rtpClient_ = std::make_shared<ObRTPClient>();
    rtpClient_->init(portInfo_->localAddress, portInfo_->address, portInfo_->port);
}

uint16_t RTPStreamPort::getStreamPort() {
    if(rtpClient_) {
        return rtpClient_->getPort();
    }
    return 0;
}

StreamProfileList RTPStreamPort::getStreamProfileList() {
    return streamProfileList_;
}

void RTPStreamPort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(!streamStarted_) {
        BEGIN_TRY_EXECUTE({
            if(rtpClient_) {
                rtpClient_->start(profile, callback);
            }
            streamStarted_ = true;
            LOG_DEBUG("{} Stream started!", utils::obStreamToStr(profile->getType()));
        })
        CATCH_EXCEPTION_AND_EXECUTE({
            stopStream();
            LOG_DEBUG("{} Stream start failed!", utils::obStreamToStr(profile->getType()));
        })
    }
    else {
        LOG_WARN("{} stream already started!", utils::obStreamToStr(profile->getType()));
    }
}

void RTPStreamPort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    LOG_DEBUG("{} Stream stoped!", utils::obStreamToStr(profile->getType()));
    stopStream();
}

void RTPStreamPort::stopAllStream() {
    stopStream();
    if(rtpClient_) {
        rtpClient_->close();
        rtpClient_.reset();
    }
}

void RTPStreamPort::startStream(MutableFrameCallback callback) {
    if(!streamStarted_) {
        BEGIN_TRY_EXECUTE({
            if(rtpClient_) {
                rtpClient_->start(nullptr, callback);
            }
            streamStarted_ = true;
            LOG_DEBUG("IMU Stream started!");
        })
        CATCH_EXCEPTION_AND_EXECUTE({
            stopStream();
            LOG_DEBUG("IMU Stream start failed!");
        })
    }
    else {
        LOG_WARN("IMU stream already started!");
    }
}

void RTPStreamPort::stopStream() {
    if(rtpClient_) {
        rtpClient_->stop();
    }
    streamStarted_ = false;
    LOG_DEBUG("Stream stop!");
}

std::shared_ptr<const SourcePortInfo> RTPStreamPort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor