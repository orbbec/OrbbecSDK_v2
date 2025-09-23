#include "PipelineHolder.hpp"


PipelineHolder::PipelineHolder(std::shared_ptr<ob::Pipeline> pipeline, OBSensorType sensorType, std::string deviceSN, int deviceIndex)
    : startStream_(false), pipeline_(pipeline), sensorType_(sensorType), deviceSN_(deviceSN), deviceIndex_(deviceIndex) {
}

PipelineHolder::~PipelineHolder() {
    release();
}

void PipelineHolder::startStream() {
    std::cout << "startStream: " << deviceSN_ << " sensorType:" << sensorType_ << std::endl;
    try {
        if(pipeline_) {
            auto profileList   = pipeline_->getStreamProfileList(sensorType_);
            auto streamProfile = profileList->getProfile(OB_PROFILE_DEFAULT)->as<ob::VideoStreamProfile>();
            frameType_         = mapFrameType(sensorType_);

            auto fps   = streamProfile->getFps();
            halfTspGap = static_cast<uint32_t>(500.0f / fps + 0.5);

            std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
            config->enableStream(streamProfile);

            pipeline_->start(config, [this](std::shared_ptr<ob::FrameSet> frameSet) {
                processFrame(frameSet);
            });
            startStream_ = true;
        }
    }
    catch(ob::Error &e) {
        std::cerr << "starting stream failed: " << deviceSN_ << std::endl;
        handleStreamError(e);
    }
}

void PipelineHolder::processFrame(std::shared_ptr<ob::FrameSet> frameSet) {
    if(!frameSet) {
        std::cerr << "Invalid frameSet received." << std::endl;
        return;
    }

    if(!startStream_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        auto obFrame = frameSet->getFrame(frameType_);
        if(obFrame) {
            if(obFrames.size() >= static_cast<size_t>(maxFrameSize_)) {
                obFrames.pop();
            }
            obFrames.push(obFrame);
        }
    }

    condVar_.notify_all();
}

bool PipelineHolder::isFrameReady() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        condVar_.wait(lock, [this]() { return !obFrames.empty() || startStream_; });
        if(startStream_ && obFrames.empty()) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<ob::Frame> PipelineHolder::frontFrame() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        condVar_.wait(lock, [this]() { return !obFrames.empty() || startStream_; });
        if(startStream_ && obFrames.empty()) {
            return nullptr;
        }
        auto frame = obFrames.front();
        return frame;
    }
}

void PipelineHolder::popFrame() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        condVar_.wait(lock, [this]() { return !obFrames.empty() || startStream_; });
        if(startStream_ && obFrames.empty()) {
            return;
        }
        obFrames.pop();
    }
}

std::shared_ptr<ob::Frame> PipelineHolder::getFrame() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        condVar_.wait(lock, [this]() { return !obFrames.empty() || startStream_; });
        if(startStream_ && obFrames.empty()) {
            return nullptr;
        }
        auto frame = obFrames.front();
        obFrames.pop();
        return frame;
    }
}

void PipelineHolder::stopStream() {
    try {
        if(pipeline_) {
            std::cout << "stopStream: " << deviceSN_ << " sensorType:" << sensorType_ << std::endl;
            startStream_ = false;
            pipeline_->stop();
        }
    }
    catch(ob::Error &e) {
        std::cerr << "stopping stream failed: " << deviceSN_ << std::endl;
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void PipelineHolder::release() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        startStream_ = false;
    }
    condVar_.notify_all();
}

void PipelineHolder::handleStreamError(const ob::Error &e) {
    std::cerr << "Function: " << e.getName() << "\nArgs: " << e.getArgs() << "\nMessage: " << e.getMessage() << "\nType: " << e.getExceptionType() << std::endl;
}

OBFrameType PipelineHolder::mapFrameType(OBSensorType sensorType) {
    switch(sensorType) {
    case OB_SENSOR_COLOR:
        return OB_FRAME_COLOR;
    case OB_SENSOR_IR:
        return OB_FRAME_IR;
    case OB_SENSOR_IR_LEFT:
        return OB_FRAME_IR_LEFT;
    case OB_SENSOR_IR_RIGHT:
        return OB_FRAME_IR_RIGHT;
    case OB_SENSOR_DEPTH:
        return OB_FRAME_DEPTH;
    default:
        return OBFrameType::OB_FRAME_UNKNOWN;
    }
}