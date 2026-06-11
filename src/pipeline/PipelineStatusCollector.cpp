// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PipelineStatusCollector.hpp"
#include "ISourcePort.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

PipelineStatusCollector::PipelineStatusCollector(IDevice *device) : device_(device) {}

PipelineStatusCollector::~PipelineStatusCollector() noexcept {
    disableHealthMonitor();
}

void PipelineStatusCollector::reportSdkStatus(uint64_t statusBit) {
    sdkStatus_.fetch_or(statusBit, std::memory_order_relaxed);
}

void PipelineStatusCollector::reportFrameReceived(OBStreamType stream) {
    std::lock_guard<std::mutex> lock(framTimeMutex_);
    lastFrameTime_[stream] = nowUsec();
}

void PipelineStatusCollector::addActivePort(std::shared_ptr<ISourcePort> port) {
    std::lock_guard<std::mutex> lock(portsMutex_);
    activePorts_.push_back(std::move(port));
}

void PipelineStatusCollector::clearActivePorts() {
    std::lock_guard<std::mutex> lock(portsMutex_);
    activePorts_.clear();
}

void PipelineStatusCollector::setExternalCollector(std::function<void()> collector) {
    externalCollector_ = std::move(collector);
}

OBPipelineStatus PipelineStatusCollector::getStatus() {
    std::unique_lock<std::mutex> lock(monitorMutex_);
    const bool                   monitorEnabled = monitorThread_.joinable() && !monitorStop_;
    if(monitorEnabled) {
        const uint64_t reqSeq    = ++monitorCollectReqSeq_;
        monitorCollectRequested_ = true;
        monitorCv_.notify_all();

        monitorCollectCv_.wait(lock, [this, reqSeq]() { return monitorStop_ || monitorCollectDoneSeq_ >= reqSeq; });
        if(monitorCollectDoneSeq_ >= reqSeq) {
            return lastStatusCache_;
        }
    }

    lock.unlock();
    return getStatusAndReset(true);
}

OBPipelineStatus PipelineStatusCollector::getStatusAndReset(bool forceDeviceQuery) {
    // Collect external status (e.g. sensor drop stats) before reading
    if(externalCollector_) {
        externalCollector_();
    }

    OBPipelineStatus result{};
    result.sdkStatus = sdkStatus_.exchange(0, std::memory_order_acq_rel);

    evaluateNoFrame(result);

    if(forceDeviceQuery || needDeviceQuery(result.sdkStatus)) {
        fetchDeviceAndDriverStatus(result, forceDeviceQuery);
    }

    result.issue = deriveIssue(result);
    return result;
}

void PipelineStatusCollector::startHealthMonitorThread() {
    auto interval  = std::chrono::milliseconds(intervalMs_);
    monitorThread_ = std::thread([this, interval]() {
        std::unique_lock<std::mutex> lock(monitorMutex_);
        while(!monitorStop_) {
            monitorCv_.wait_for(lock, interval, [this]() { return monitorStop_ || monitorCollectRequested_; });
            if(monitorStop_) {
                break;
            }

            const bool     onDemandCollection = monitorCollectRequested_;
            const uint64_t requestSeq         = monitorCollectReqSeq_;
            monitorCollectRequested_          = false;

            // Temporarily unlock to avoid holding monitorMutex_ during status collection
            auto cb       = monitorCallback_;
            auto userData = monitorUserData_;
            lock.unlock();

            auto status = getStatusAndReset(onDemandCollection);

            lock.lock();
            lastStatusCache_ = status;

            if(onDemandCollection) {
                if(requestSeq > monitorCollectDoneSeq_) {
                    monitorCollectDoneSeq_ = requestSeq;
                }
                monitorCollectCv_.notify_all();
                continue;
            }

            if(status.sdkStatus || status.devStatus || status.drvStatus) {
                if(cb) {
                    lock.unlock();
                    cb(status, userData);
                    lock.lock();
                }
            }
        }
    });
}

void PipelineStatusCollector::enableHealthMonitor(ob_pipeline_status_callback callback, void *userData, uint32_t intervalMs) {
    disableHealthMonitor();

    std::lock_guard<std::mutex> lock(monitorMutex_);
    monitorCallback_         = callback;
    monitorUserData_         = userData;
    monitorStop_             = false;
    monitorCollectRequested_ = false;
    monitorCollectReqSeq_    = 0;
    monitorCollectDoneSeq_   = 0;
    lastStatusCache_         = {};
    intervalMs_              = intervalMs;
    monitorEnable_           = true;

    startHealthMonitorThread();
}

void PipelineStatusCollector::disableHealthMonitor() {
    {
        std::lock_guard<std::mutex> lock(monitorMutex_);
        monitorStop_ = true;
        monitorCv_.notify_all();
        monitorCollectCv_.notify_all();
    }
    if(monitorThread_.joinable()) {
        monitorThread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(monitorMutex_);
        monitorCallback_         = nullptr;
        monitorUserData_         = nullptr;
        monitorCollectRequested_ = false;
        monitorEnable_           = false;
    }
}

void PipelineStatusCollector::reset() {
    ob_pipeline_status_callback callback{};
    void                       *userData{};
    bool                        enable{};

    {
        // save state
        std::lock_guard<std::mutex> lock(monitorMutex_);
        callback = monitorCallback_;
        userData = monitorUserData_;
        enable   = monitorEnable_.load();
    }
    // disable health monitor
    disableHealthMonitor();
    // reset status
    sdkStatus_.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(framTimeMutex_);
        lastFrameTime_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(devFetchMutex_);
        cachedDrvStatus_  = 0;
        lastDrvFetchTime_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(monitorMutex_);
        lastStatusCache_         = {};
        monitorCollectRequested_ = false;
        monitorCollectReqSeq_    = 0;
        monitorCollectDoneSeq_   = 0;
        // restore state
        monitorEnable_   = enable;
        monitorCallback_ = callback;
        monitorUserData_ = userData;
    }
}

void PipelineStatusCollector::resume() {
    std::lock_guard<std::mutex> lock(monitorMutex_);
    if(monitorEnable_.load() && monitorStop_) {
        monitorStop_ = false;
        startHealthMonitorThread();
    }
}

void PipelineStatusCollector::setNoFrameThresholdUsec(uint64_t thresholdUsec) {
    noFrameThresholdUsec_ = thresholdUsec;
}

void PipelineStatusCollector::setDevFetchIntervalMs(uint64_t intervalMs) {
    std::lock_guard<std::mutex> lock(devFetchMutex_);
    devFetchInterval_ = intervalMs;
}

void PipelineStatusCollector::evaluateNoFrame(OBPipelineStatus &status) {
    auto                        now = nowUsec();
    std::lock_guard<std::mutex> lock(framTimeMutex_);
    for(auto &kv: lastFrameTime_) {
        uint64_t t = kv.second;
        if(t == 0) {
            continue;
        }
        if(now - t > noFrameThresholdUsec_) {
            status.sdkStatus |= OB_SDK_STATUS_STREAM_NO_FRAME;
            break;
        }
    }
}

bool PipelineStatusCollector::needDeviceQuery(uint64_t sdkStatus) {
    return sdkStatus != 0;
}

void PipelineStatusCollector::fetchDeviceAndDriverStatus(OBPipelineStatus &status, bool forceFetch) {
    // Fetch device error state via Property 5524. The cache/throttle is owned by the
    // device itself (shared across all pipelines bound to it); forceFetch=true requires
    // the latest value, otherwise reuse a cached value up to devFetchInterval_ old.
    uint64_t intervalMs;
    {
        std::lock_guard<std::mutex> lock(devFetchMutex_);
        intervalMs = devFetchInterval_;
    }

    uint64_t devStatus        = 0;
    uint64_t maxDevCacheAgeMs = forceFetch ? 0 : intervalMs;
    try {
        device_->fetchDeviceErrorState(maxDevCacheAgeMs);
        devStatus = device_->getDeviceErrorState();
        // Clear the most significant bit (MSB), which indicates the state info cache flag
        devStatus &= ~(1ULL << 63);
    }
    catch(...) {
        devStatus = 0;
    }

    status.devStatus = devStatus;

    // Throttle the driver status query the same way: getDriverStatus() may issue a kernel
    // ioctl on GMSL ports, so reuse the cached value within devFetchInterval_ unless forced.
    {
        std::lock_guard<std::mutex> lock(devFetchMutex_);
        if(forceFetch || lastDrvFetchTime_ == 0 || utils::getSteadyTimeMs() - lastDrvFetchTime_ >= intervalMs) {
            cachedDrvStatus_  = fetchPortDriverStatus();
            lastDrvFetchTime_ = utils::getSteadyTimeMs();
        }
        status.drvStatus = cachedDrvStatus_;
    }
}

uint64_t PipelineStatusCollector::fetchPortDriverStatus() {
    uint64_t                    drvStatus = 0;
    std::lock_guard<std::mutex> lock(portsMutex_);
    for(auto &port: activePorts_) {
        drvStatus |= port->getDriverStatus();
    }
    return drvStatus;
}

OBPipelineIssue PipelineStatusCollector::deriveIssue(const OBPipelineStatus &s) {
    uint32_t issue = OB_PIPELINE_ISSUE_NONE;
    if(s.sdkStatus) {
        issue |= OB_PIPELINE_ISSUE_SDK;
    }
    if(s.devStatus) {
        issue |= OB_PIPELINE_ISSUE_FW | OB_PIPELINE_ISSUE_HW;
    }
    if(s.drvStatus) {
        issue |= OB_PIPELINE_ISSUE_DRIVER;
    }
    return static_cast<OBPipelineIssue>(issue);
}

uint64_t PipelineStatusCollector::nowUsec() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
}

}  // namespace libobsensor
