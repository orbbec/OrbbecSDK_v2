#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {
RTPStreamPort::RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo)
    : portInfo_(portInfo), destroy_(0), streamStarted_(false) {}

RTPStreamPort::~RTPStreamPort() {

    // 1. 关流, 关闭rtsp客户端
    TRY_EXECUTE(stopAllStream());
    // 2. 释放调度器

    // 3. live555 env 释放
}

StreamProfileList RTPStreamPort::getStreamProfileList() {
    return streamProfileList_;
}

void RTPStreamPort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(streamStarted_) {
        LOG_WARN("Stream already started!");
    }
    else {
        BEGIN_TRY_EXECUTE({
            // createClient(profile, callback);
            // currentRtspClient_->startStream();
        })
        CATCH_EXCEPTION_AND_EXECUTE({
            // closeClient();
            throw;
        })
        streamStarted_ = true;
        LOG_DEBUG("Stream started!");
    }
}
void RTPStreamPort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    stopStream();
}

void RTPStreamPort::stopAllStream() {
    // 当前RTPStreamPort只支持一路数据流
    stopStream();
}

void RTPStreamPort::stopStream() {
    if(streamStarted_) {
        BEGIN_TRY_EXECUTE({ 
            //currentRtspClient_->stopStream(); 
            
        })
        CATCH_EXCEPTION_AND_EXECUTE({
            closeClient();
            streamStarted_ = false;
            throw;
        })
        closeClient();
        streamStarted_ = false;
        LOG_DEBUG("Stream stoped!");
    }
    else {
        LOG_WARN("Stream have not been started!");
    }
}

void RTPStreamPort::createClient(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    destroy_              = 0;
    eventLoopThread_      = std::thread([&]() { 
        //live555Env_->taskScheduler().doEventLoop(&destroy_); 
    });

    currentStreamProfile_ = profile;
    auto vsp              = currentStreamProfile_->as<VideoStreamProfile>();

    // std::string formatStr = std::to_string(vsp->getWidth()) + "_" + std::to_string(vsp->getHeight()) + "_" + std::to_string(vsp->getFps()) + "_"
    //                         + mapFormatToString(vsp->getFormat());
    // std::string url =
    //     std::string("rtsp://") + portInfo_->address + ":" + std::to_string(portInfo_->port) + "/" + mapStreamTypeToString(vsp->getType()) + "/" + formatStr;
    // currentRtspClient_ = ObRTSPClient::createNew(profile, *live555Env_, url.c_str(), callback, 1);
    // LOG_DEBUG("ObRTSPClient created! url={}", url);
}

void RTPStreamPort::closeClient() {
    destroy_ = 1;
    if(eventLoopThread_.joinable()) {
        eventLoopThread_.join();
    }
    std::string url = "";
    // if(currentRtspClient_) {
    //     url = currentRtspClient_->url();
    //     // 需要在client关闭前关闭事务线程，否则会偶发crash
    //     Medium::close(currentRtspClient_);
    //     currentRtspClient_ = nullptr;
    // }
    // LOG_DEBUG("ObRTSPClient close!  url={}", url);
}

std::shared_ptr<const SourcePortInfo> RTPStreamPort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor