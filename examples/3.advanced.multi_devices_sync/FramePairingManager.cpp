#include "FramePairingManager.hpp"
#include <chrono>
#include <iostream>
#include <sstream>

static constexpr size_t MAX_QUEUE_SIZE = 600;
static constexpr size_t CSV_BUF_SIZE   = 65536;

FramePairingManager::FramePairingManager() {}

FramePairingManager::~FramePairingManager() {
    release();
}

void FramePairingManager::init(const std::vector<std::string> &deviceSNs, const std::string &outputDir) {
    // Close any previously opened sinks before rebuilding (supports re-init).
    for(auto &s: sinks_) {
        if(!s)
            continue;
        std::lock_guard<std::mutex> lk(s->mtx);
        if(s->csv.is_open()) {
            s->csv.flush();
            s->csv.close();
        }
    }
    destroy_.store(false);

    deviceCount_ = deviceSNs.size();
    deviceSNs_   = deviceSNs;
    sinks_.clear();
    sinks_.reserve(deviceCount_ * 2);
    for(size_t d = 0; d < deviceCount_; ++d) {
        sinks_.emplace_back(std::make_shared<SensorSink>());
        sinks_.emplace_back(std::make_shared<SensorSink>());
    }

    for(size_t d = 0; d < deviceCount_; ++d) {
        {
            auto &s = sinks_[sinkIndex(d, false)];
            s->fileBuf.assign(CSV_BUF_SIZE, 0);
            openSinkCsv(*s, outputDir + "/sync_depth_dev" + std::to_string(d) + "_" + deviceSNs_[d] + ".csv");
        }
        {
            auto &s = sinks_[sinkIndex(d, true)];
            s->fileBuf.assign(CSV_BUF_SIZE, 0);
            openSinkCsv(*s, outputDir + "/sync_color_dev" + std::to_string(d) + "_" + deviceSNs_[d] + ".csv");
        }
    }
}

void FramePairingManager::openSinkCsv(SensorSink &sink, const std::string &path) {
    std::lock_guard<std::mutex> lk(sink.mtx);
    // pubsetbuf MUST be called before open() to take effect.
    if(!sink.fileBuf.empty()) {
        sink.csv.rdbuf()->pubsetbuf(sink.fileBuf.data(), sink.fileBuf.size());
    }
    sink.csv.open(path);
    if(sink.csv.is_open()) {
        writeCsvHeader(sink.csv);
        std::cout << "CSV: " << path << std::endl;
    }
    else {
        std::cerr << "Failed to open CSV: " << path << std::endl;
    }
}

void FramePairingManager::writeCsvHeader(std::ostream &csv) {
    csv << "row_id,sw_frame_num,hw_frame_num,system_ts_us,device_ts_us,global_ts_us\n";
}

void FramePairingManager::writeCsvRow(std::ostream &csv, uint64_t rowId, const DeviceTimestamp &t) {
    csv << rowId << "," << t.swFrameNum << "," << t.hwFrameNum << "," << t.systemTs << "," << t.deviceTs << "," << t.globalTs << "\n";
}

void FramePairingManager::pushDepthFrame(int deviceIndex, int64_t hwFrameNum, int64_t swFrameNum, int64_t systemTs, int64_t deviceTs, int64_t globalTs) {
    if(!recording_.load()) {
        return;
    }
    if(static_cast<size_t>(deviceIndex) >= deviceCount_) {
        return;
    }
    auto                       &s = sinks_[sinkIndex(deviceIndex, false)];
    std::lock_guard<std::mutex> lk(s->mtx);
    if(s->queue.size() >= MAX_QUEUE_SIZE) {
        s->queue.pop_front();
    }
    DeviceTimestamp t;
    t.hwFrameNum = hwFrameNum;
    t.swFrameNum = swFrameNum;
    t.systemTs   = systemTs;
    t.deviceTs   = deviceTs;
    t.globalTs   = globalTs;
    s->queue.push_back(t);
    ++s->capturedCounter;
}

void FramePairingManager::pushColorFrame(int deviceIndex, int64_t hwFrameNum, int64_t swFrameNum, int64_t systemTs, int64_t deviceTs, int64_t globalTs) {
    if(!recording_.load()) {
        return;
    }
    if(static_cast<size_t>(deviceIndex) >= deviceCount_) {
        return;
    }
    auto                       &s = sinks_[sinkIndex(deviceIndex, true)];
    std::lock_guard<std::mutex> lk(s->mtx);
    if(s->queue.size() >= MAX_QUEUE_SIZE) {
        s->queue.pop_front();
    }
    DeviceTimestamp t;
    t.hwFrameNum = hwFrameNum;
    t.swFrameNum = swFrameNum;
    t.systemTs   = systemTs;
    t.deviceTs   = deviceTs;
    t.globalTs   = globalTs;
    s->queue.push_back(t);
    ++s->capturedCounter;
}

void FramePairingManager::flushSink(SensorSink &sink) {
    std::deque<DeviceTimestamp> batch;
    uint64_t                    startRowId = 0;
    {
        std::lock_guard<std::mutex> lk(sink.mtx);
        if(!sink.csv.is_open()) {
            sink.queue.clear();
            return;
        }
        batch.swap(sink.queue);
        startRowId = sink.rowId;
        sink.rowId += batch.size();
    }
    if(batch.empty()) {
        return;
    }
    std::ostringstream batchSs;
    uint64_t           rowId = startRowId;
    for(auto &t: batch) {
        writeCsvRow(batchSs, rowId++, t);
    }
    {
        std::lock_guard<std::mutex> lk(sink.mtx);
        if(sink.csv.is_open()) {
            sink.csv << batchSs.str();
        }
    }
    sink.rowsWritten += batch.size();
}

void FramePairingManager::flushThreadFunc() {
    std::unique_lock<std::mutex> lk(flushCvMtx_);
    while(!flushThreadStop_.load()) {
        flushCv_.wait_for(lk, std::chrono::milliseconds(100));
        if(!recording_.load()) {
            continue;
        }
        for(auto &s: sinks_) {
            if(s)
                flushSink(*s);
        }
    }
}

void FramePairingManager::startBackgroundFlush() {
    if(flushThreadRunning_.exchange(true)) {
        return;
    }
    flushThreadStop_.store(false);
    flushThread_ = std::thread(&FramePairingManager::flushThreadFunc, this);
}

void FramePairingManager::stopBackgroundFlush() {
    if(!flushThreadRunning_.exchange(false)) {
        return;
    }
    flushThreadStop_.store(true);
    flushCv_.notify_all();
    if(flushThread_.joinable()) {
        flushThread_.join();
    }
    // Drain anything still queued.
    for(auto &s: sinks_) {
        if(s)
            flushSink(*s);
    }
}

void FramePairingManager::setRecording(bool v) {
    recording_.store(v);
}

void FramePairingManager::resetCounters() {
    for(auto &s: sinks_) {
        std::lock_guard<std::mutex> lk(s->mtx);
        s->capturedCounter = 0;
        s->rowId           = 0;
        s->rowsWritten.store(0);
    }
}

uint64_t FramePairingManager::getColorCapturedCount(size_t deviceIndex) const {
    if(deviceIndex >= deviceCount_) {
        return 0;
    }
    auto                       &s = sinks_[sinkIndex(deviceIndex, true)];
    std::lock_guard<std::mutex> lk(s->mtx);
    return s->capturedCounter;
}

uint64_t FramePairingManager::getDepthCapturedCount(size_t deviceIndex) const {
    if(deviceIndex >= deviceCount_) {
        return 0;
    }
    auto                       &s = sinks_[sinkIndex(deviceIndex, false)];
    std::lock_guard<std::mutex> lk(s->mtx);
    return s->capturedCounter;
}

uint64_t FramePairingManager::getColorRowsWritten(size_t deviceIndex) const {
    if(deviceIndex >= deviceCount_) {
        return 0;
    }
    return sinks_[sinkIndex(deviceIndex, true)]->rowsWritten.load();
}

uint64_t FramePairingManager::getDepthRowsWritten(size_t deviceIndex) const {
    if(deviceIndex >= deviceCount_) {
        return 0;
    }
    return sinks_[sinkIndex(deviceIndex, false)]->rowsWritten.load();
}

void FramePairingManager::release() {
    stopBackgroundFlush();
    if(destroy_.exchange(true)) {
        return;
    }
    for(auto &s: sinks_) {
        if(!s)
            continue;
        std::lock_guard<std::mutex> lk(s->mtx);
        if(s->csv.is_open()) {
            s->csv.flush();
            s->csv.close();
        }
    }
}

FramePairingManager gTimestampBuffer;
