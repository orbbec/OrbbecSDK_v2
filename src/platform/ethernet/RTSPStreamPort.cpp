// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RTSPStreamPort.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

#include <map>
namespace libobsensor {
std::string mapFormatToString(OBFormat format) {
    static std::map<OBFormat, std::string> formatToStringMap = {
        { OB_FORMAT_YUYV, "yuyv" }, { OB_FORMAT_YUY2, "yuyv" }, { OB_FORMAT_UYVY, "uyvy" }, { OB_FORMAT_NV12, "nv12" }, { OB_FORMAT_NV21, "nv21" },
        { OB_FORMAT_MJPG, "mjpg" }, { OB_FORMAT_H264, "h264" }, { OB_FORMAT_H265, "h265" }, { OB_FORMAT_Y16, "y16" },   { OB_FORMAT_Y8, "y8" },
        { OB_FORMAT_Y10, "y10" },   { OB_FORMAT_Y11, "y11" },   { OB_FORMAT_Y12, "y12" },   { OB_FORMAT_GRAY, "y8" },   { OB_FORMAT_HEVC, "h265" },
        { OB_FORMAT_I420, "i420" }, { OB_FORMAT_RLE, "rle" },   { OB_FORMAT_RGB, "rgb" },   { OB_FORMAT_BGR, "bgr" },   { OB_FORMAT_Y14, "y14" },
        { OB_FORMAT_BGRA, "bgra" }, { OB_FORMAT_RGBA, "rgba" }, { OB_FORMAT_RVL, "rvl" },   { OB_FORMAT_BYR2, "byr2" }, { OB_FORMAT_RW16, "rw16" },
    };
    return formatToStringMap[format];
}

std::string mapStreamTypeToString(OBStreamType streamType) {
    static std::map<OBStreamType, std::string> streamTypeToStringMap = {
        { OB_STREAM_IR, "ir" },           { OB_STREAM_DEPTH, "depth" },       { OB_STREAM_COLOR, "color" },
        { OB_STREAM_IR_LEFT, "ir_left" }, { OB_STREAM_IR_RIGHT, "ir_right" },
    };
    return streamTypeToStringMap[streamType];
}

RTSPStreamPort::RTSPStreamPort(std::shared_ptr<const RTSPStreamPortInfo> portInfo)
    : portInfo_(portInfo), destroy_(0), streamStarted_(false), currentRtspClient_(nullptr) {
    taskScheduler_ = BasicTaskScheduler::createNew();
    live555Env_    = ObUsageEnvironment::createNew(*taskScheduler_);
}

RTSPStreamPort::~RTSPStreamPort() noexcept {

    // 1. Turn off the flow and close the rtsp client
    TRY_EXECUTE(stopAllStream());  // try stop all stream

    // 2. Release the scheduler
    delete taskScheduler_;
    taskScheduler_ = NULL;

    // 3. live555 env release
    live555Env_->reclaim();
    live555Env_ = NULL;
}

StreamProfileList RTSPStreamPort::getStreamProfileList() {
    return {};  // retuen empty list since the stream profile will be fetched via vendor specific command on the device bussiness layer
}

void RTSPStreamPort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(streamStarted_) {
        LOG_WARN("Stream already started!");
    }
    else {
        BEGIN_TRY_EXECUTE({
            createClient(profile, callback);
            currentRtspClient_->startStream();
        })
        CATCH_EXCEPTION_AND_EXECUTE({
            closeClient();
            throw;
        })
        streamStarted_ = true;
        LOG_DEBUG("Stream started!");
    }
}
void RTSPStreamPort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    utils::unusedVar(profile);
    stopStream();
}

void RTSPStreamPort::stopAllStream() {
    // Currently RTSPStreamPort only supports one data stream
    stopStream();
}

void RTSPStreamPort::stopStream() {
    if(streamStarted_) {
        BEGIN_TRY_EXECUTE({ currentRtspClient_->stopStream(); })
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

void RTSPStreamPort::createClient(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    destroy_              = 0;
    eventLoopThread_      = std::thread([&]() { live555Env_->taskScheduler().doEventLoop(&destroy_); });
    currentStreamProfile_ = profile;
    auto vsp              = currentStreamProfile_->as<VideoStreamProfile>();

    std::string formatStr = std::to_string(vsp->getWidth()) + "_" + std::to_string(vsp->getHeight()) + "_" + std::to_string(vsp->getFps()) + "_"
                            + mapFormatToString(vsp->getFormat());
    std::string url =
        std::string("rtsp://") + portInfo_->address + ":" + std::to_string(portInfo_->port) + "/" + mapStreamTypeToString(vsp->getType()) + "/" + formatStr;
    currentRtspClient_ = ObRTSPClient::createNew(profile, *live555Env_, url.c_str(), callback, 1);
    LOG_DEBUG("ObRTSPClient created! url={}", url);
}

void RTSPStreamPort::closeClient() {
    destroy_ = 1;
    if(eventLoopThread_.joinable()) {
        eventLoopThread_.join();
    }
    std::string url = "";
    if(currentRtspClient_) {
        url = currentRtspClient_->url();
        // The transaction thread needs to be closed before the client is closed, otherwise it will crash occasionally.
        Medium::close(currentRtspClient_);
        currentRtspClient_ = nullptr;
    }
    LOG_DEBUG("ObRTSPClient close!  url={}", url);
}

std::shared_ptr<const SourcePortInfo> RTSPStreamPort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor

