#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct DeviceTimestamp {
    int64_t hwFrameNum = -1;
    int64_t swFrameNum = -1;
    int64_t systemTs   = 0;
    int64_t deviceTs   = 0;
    int64_t globalTs   = 0;
};

class FramePairingManager {
public:
    FramePairingManager();
    ~FramePairingManager();

    void init(const std::vector<std::string> &deviceSNs, const std::string &outputDir = ".");

    void pushColorFrame(int deviceIndex, int64_t hwFrameNum, int64_t swFrameNum, int64_t systemTs, int64_t deviceTs, int64_t globalTs);
    void pushDepthFrame(int deviceIndex, int64_t hwFrameNum, int64_t swFrameNum, int64_t systemTs, int64_t deviceTs, int64_t globalTs);

    void     setRecording(bool v);
    void     resetCounters();
    uint64_t getColorCapturedCount(size_t deviceIndex) const;
    uint64_t getDepthCapturedCount(size_t deviceIndex) const;
    uint64_t getColorRowsWritten(size_t deviceIndex) const;
    uint64_t getDepthRowsWritten(size_t deviceIndex) const;

    void startBackgroundFlush();
    void stopBackgroundFlush();

    void release();

private:
    struct SensorSink {
        std::deque<DeviceTimestamp> queue;
        uint64_t                    capturedCounter = 0;
        mutable std::mutex          mtx;
        std::ofstream               csv;
        uint64_t                    rowId = 0;
        std::atomic<uint64_t>       rowsWritten{ 0 };
        std::vector<char>           fileBuf;
    };

    size_t sinkIndex(size_t deviceIndex, bool isColor) const {
        return deviceIndex * 2 + (isColor ? 1 : 0);
    }

    void openSinkCsv(SensorSink &sink, const std::string &path);
    void writeCsvHeader(std::ostream &csv);
    void writeCsvRow(std::ostream &csv, uint64_t rowId, const DeviceTimestamp &t);
    void flushSink(SensorSink &sink);
    void flushThreadFunc();

    size_t                                   deviceCount_ = 0;
    std::vector<std::string>                 deviceSNs_;
    std::vector<std::shared_ptr<SensorSink>> sinks_;  // deviceCount_ * 2

    std::atomic<bool> recording_{ false };
    std::atomic<bool> destroy_{ false };

    std::thread             flushThread_;
    std::mutex              flushCvMtx_;
    std::condition_variable flushCv_;
    std::atomic<bool>       flushThreadRunning_{ false };
    std::atomic<bool>       flushThreadStop_{ false };
};

extern FramePairingManager gTimestampBuffer;
