#pragma once
#include <atomic>
#include <functional>
#include <libobsensor/ObSensor.hpp>
#include <mutex>
#include <string>

struct StreamProfileRequest {
    int         width  = 0;
    int         height = 0;
    double      fps    = 0;
    std::string format;
};

OBFormat stringToOBFormat(const std::string &formatString);

class PipelineHolder {
public:
    using FrameCallback = std::function<void(std::shared_ptr<ob::FrameSet>)>;

    PipelineHolder(std::shared_ptr<ob::Device> device, int deviceIndex);
    ~PipelineHolder();

    void startStream();
    void stopStream();

    void setFrameCallback(FrameCallback cb);
    void setStreamConfig(const StreamProfileRequest &depth, const StreamProfileRequest &color);

    std::shared_ptr<ob::FrameSet> getLatestFrameSet();

    bool isStreaming() const {
        return streaming_.load();
    }

    std::shared_ptr<ob::Pipeline> getPipeline() const {
        return pipeline_;
    }

    std::string getSerialNumber() const {
        return deviceSN_;
    }

    int getDeviceIndex() const {
        return deviceIndex_;
    }

private:
    void onFrameSet(std::shared_ptr<ob::FrameSet> frameSet);

    std::shared_ptr<ob::Device>   device_;
    std::shared_ptr<ob::Pipeline> pipeline_;
    std::string                   deviceSN_;
    int                           deviceIndex_;

    StreamProfileRequest depthRequest_;
    StreamProfileRequest colorRequest_;

    std::atomic<bool> streaming_{ false };

    std::mutex                    frameMutex_;
    std::shared_ptr<ob::FrameSet> latestFrameSet_;

    FrameCallback userCallback_;
};
