#include "PipelineHolder.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>

OBFormat stringToOBFormat(const std::string &formatString) {
    static const std::unordered_map<std::string, OBFormat> formatMap = {
        { "OB_FORMAT_ANY", OB_FORMAT_ANY },     { "OB_FORMAT_UNKNOWN", OB_FORMAT_UNKNOWN },
        { "OB_FORMAT_YUYV", OB_FORMAT_YUYV },   { "OB_FORMAT_YUY2", OB_FORMAT_YUY2 },
        { "OB_FORMAT_UYVY", OB_FORMAT_UYVY },   { "OB_FORMAT_NV12", OB_FORMAT_NV12 },
        { "OB_FORMAT_NV21", OB_FORMAT_NV21 },   { "OB_FORMAT_MJPG", OB_FORMAT_MJPG },
        { "OB_FORMAT_H264", OB_FORMAT_H264 },   { "OB_FORMAT_H265", OB_FORMAT_H265 },
        { "OB_FORMAT_Y16", OB_FORMAT_Y16 },     { "OB_FORMAT_Y8", OB_FORMAT_Y8 },
        { "OB_FORMAT_Y10", OB_FORMAT_Y10 },     { "OB_FORMAT_Y11", OB_FORMAT_Y11 },
        { "OB_FORMAT_Y12", OB_FORMAT_Y12 },     { "OB_FORMAT_GRAY", OB_FORMAT_GRAY },
        { "OB_FORMAT_HEVC", OB_FORMAT_HEVC },   { "OB_FORMAT_I420", OB_FORMAT_I420 },
        { "OB_FORMAT_POINT", OB_FORMAT_POINT }, { "OB_FORMAT_RGB_POINT", OB_FORMAT_RGB_POINT },
        { "OB_FORMAT_RLE", OB_FORMAT_RLE },     { "OB_FORMAT_RGB", OB_FORMAT_RGB },
        { "OB_FORMAT_BGR", OB_FORMAT_BGR },     { "OB_FORMAT_Y14", OB_FORMAT_Y14 },
        { "OB_FORMAT_BGRA", OB_FORMAT_BGRA },   { "OB_FORMAT_Z16", OB_FORMAT_Z16 },
        { "OB_FORMAT_YV12", OB_FORMAT_YV12 },   { "OB_FORMAT_BA81", OB_FORMAT_BA81 },
        { "OB_FORMAT_RGBA", OB_FORMAT_RGBA },   { "OB_FORMAT_BYR2", OB_FORMAT_BYR2 },
        { "OB_FORMAT_RW16", OB_FORMAT_RW16 },   { "OB_FORMAT_Y12C4", OB_FORMAT_Y12C4 },
    };
    auto it = formatMap.find(formatString);
    if(it != formatMap.end()) {
        return it->second;
    }
    throw std::invalid_argument("Unrecognized stream format: " + formatString);
}

PipelineHolder::PipelineHolder(std::shared_ptr<ob::Device> device, int deviceIndex) : device_(device), deviceIndex_(deviceIndex) {
    if(device_) {
        pipeline_ = std::make_shared<ob::Pipeline>(device_);
        deviceSN_ = device_->getDeviceInfo()->serialNumber();
    }
}

PipelineHolder::~PipelineHolder() {
    stopStream();
}

void PipelineHolder::setFrameCallback(FrameCallback cb) {
    userCallback_ = cb;
}

void PipelineHolder::setStreamConfig(const StreamProfileRequest &depth, const StreamProfileRequest &color) {
    depthRequest_ = depth;
    colorRequest_ = color;
}

void PipelineHolder::startStream() {
    if(streaming_.load() || !pipeline_) {
        return;
    }

    try {
        std::shared_ptr<ob::Config> config      = std::make_shared<ob::Config>();
        OBFormat                    depthFormat = depthRequest_.format.empty() ? OB_FORMAT_ANY : stringToOBFormat(depthRequest_.format);
        config->enableVideoStream(OB_SENSOR_DEPTH, depthRequest_.width, depthRequest_.height, static_cast<uint32_t>(depthRequest_.fps), depthFormat);
        OBFormat colorFormat = colorRequest_.format.empty() ? OB_FORMAT_ANY : stringToOBFormat(colorRequest_.format);
        config->enableVideoStream(OB_SENSOR_COLOR, colorRequest_.width, colorRequest_.height, static_cast<uint32_t>(colorRequest_.fps), colorFormat);

        pipeline_->start(config, [this](std::shared_ptr<ob::FrameSet> frameSet) { onFrameSet(frameSet); });

        streaming_ = true;
        std::cout << "startStream: " << deviceSN_ << " (device #" << deviceIndex_ << ")" << std::endl;
    }
    catch(ob::Error &e) {
        std::cerr << "starting stream failed: " << deviceSN_ << std::endl;
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
                  << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void PipelineHolder::onFrameSet(std::shared_ptr<ob::FrameSet> frameSet) {
    if(!frameSet) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(frameMutex_);
        latestFrameSet_ = frameSet;
    }
    if(userCallback_) {
        userCallback_(frameSet);
    }
}

std::shared_ptr<ob::FrameSet> PipelineHolder::getLatestFrameSet() {
    std::lock_guard<std::mutex> lk(frameMutex_);
    return latestFrameSet_;
}

void PipelineHolder::stopStream() {
    if(!streaming_.exchange(false) || !pipeline_) {
        return;
    }

    try {
        std::cout << "stopStream: " << deviceSN_ << " (device #" << deviceIndex_ << ")" << std::endl;
        pipeline_->stop();
    }
    catch(ob::Error &e) {
        std::cerr << "stopping stream failed: " << deviceSN_ << std::endl;
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
                  << "\ntype:" << e.getExceptionType() << std::endl;
    }

    std::lock_guard<std::mutex> lk(frameMutex_);
    latestFrameSet_.reset();
}
