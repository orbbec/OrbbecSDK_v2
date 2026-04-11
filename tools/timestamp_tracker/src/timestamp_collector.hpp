// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "cmdline_parser.hpp"
#include "csv_writer.hpp"
#include <libobsensor/ObSensor.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>

namespace libobsensor {
namespace tools {

struct TimedFrameSet {
    std::shared_ptr<ob::FrameSet> frameSet;
    uint64_t                      recvTimeUs;  // system_clock microseconds, captured at pipeline callback entry
};

class TimestampCollector {
public:
    explicit TimestampCollector(std::shared_ptr<ob::Device> device);
    ~TimestampCollector();

    TimestampCollector(const TimestampCollector &)            = delete;
    TimestampCollector &operator=(const TimestampCollector &) = delete;

    bool start(const CmdLineConfig &config);
    void stop();
    bool isRunning() const;

    std::shared_ptr<ob::Device> getDevice() const;
    std::string                 getDeviceInfoStr() const;
    std::vector<std::string>    getOpenedFiles() const;

private:
    void processFrames();
    void onFrameSet(std::shared_ptr<ob::FrameSet> frameSet, uint64_t recvTimeUs);
    bool setupSensors(std::shared_ptr<ob::Config> &obConfig, const CmdLineConfig &toolConfig);

private:
    std::shared_ptr<ob::Device>   device_;
    std::shared_ptr<ob::Pipeline> pipeline_;
    std::string                   deviceName_;
    std::string                   serialNumber_;

    std::map<OBSensorType, std::string>                enabledSensors_;  // value = sensorName
    std::map<OBSensorType, std::shared_ptr<CsvWriter>> writers_;
    std::vector<std::string>                           openedFiles_;  // collected on stop()

    std::atomic<bool>         running_{ false };
    std::thread               processThread_;
    std::mutex                frameQueueMutex_;
    std::condition_variable   frameQueueCv_;
    std::queue<TimedFrameSet> frameQueue_;
};

}  // namespace tools
}  // namespace libobsensor
