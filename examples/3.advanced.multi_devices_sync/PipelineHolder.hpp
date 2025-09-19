#pragma once
#include <libobsensor/ObSensor.hpp>
#include <iostream>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>


class PipelineHolder {
public:
    PipelineHolder(std::shared_ptr<ob::Pipeline> pipeline, OBSensorType sensorType, std::string deviceSN, int deviceIndex);
    ~PipelineHolder();


public:
    void startStream();

    void processFrame(std::shared_ptr<ob::FrameSet> frameSet); 

    bool isFrameReady();

    std::shared_ptr<ob::Frame> frontFrame();

    void popFrame();

    std::shared_ptr<ob::Frame> getFrame();

    void stopStream();

    void release();

    void handleStreamError(const ob::Error &e);

    OBFrameType mapFrameType(OBSensorType sensorType);

    std::string getSerialNumber() {
        return deviceSN_;
    }

    OBSensorType getSensorType() {
        return sensorType_;
    }

    OBFrameType getFrameType() {
        return frameType_;
    }

    int getDeviceIndex(){
        return deviceIndex_;
    }

   int getFrameQueueSize() {
	       std::lock_guard<std::mutex> lock(queueMutex_);
	        return static_cast<int>(obFrames.size());
    }
private:
    bool startStream_;

    std::shared_ptr<ob::Pipeline> pipeline_;
    OBSensorType                  sensorType_;
    OBFrameType                   frameType_;
    std::string                   deviceSN_;
    int                           deviceIndex_;

    std::condition_variable condVar_;
    std::mutex              queueMutex_;
    uint32_t                maxFrameSize_ = 16;


    std::queue<std::shared_ptr<ob::Frame>> obFrames;

public:
    uint32_t halfTspGap;
};
