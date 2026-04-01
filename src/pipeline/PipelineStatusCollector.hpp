// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "IPipelineStatusCollector.hpp"
#include "IDevice.hpp"

#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include <functional>
#include <thread>

namespace libobsensor {

class ISourcePort;

class PipelineStatusCollector : public IPipelineStatusCollector {
public:
    /**
     * @brief Construct a pipeline status collector.
     * @param[in] device Underlying device used for device-level status queries.
     */
    explicit PipelineStatusCollector(IDevice *device);

    /**
     * @brief Destroy the collector and stop health monitoring if running.
     */
    ~PipelineStatusCollector() noexcept override;

    /**
     * @brief Report SDK-level status bits.
     * @param[in] statusBit SDK status bitmask to OR into the accumulator.
     */
    void reportSdkStatus(uint64_t statusBit) override;

    /**
     * @brief Report that a frame is received for a stream.
     * @param[in] stream Stream type of the received frame.
     */
    void reportFrameReceived(OBStreamType stream) override;

    /**
     * @brief Register an active source port for driver-status aggregation.
     * @param[in] port Source port to register.
     */
    void addActivePort(std::shared_ptr<ISourcePort> port);

    /**
     * @brief Clear all active source ports.
     */
    void clearActivePorts();

    /**
     * @brief Set an external collector callback.
     * @param[in] collector Callback invoked before each status collection, typically to gather sensor drop statistics.
     */
    void setExternalCollector(std::function<void()> collector);

    /**
     * @brief Get pipeline status.
     * @details
     * - If health monitor is disabled, collect and reset status immediately.
     * - If health monitor is enabled, trigger one monitor-thread collection and return cached latest status.
     * @return Current pipeline status snapshot.
     */
    OBPipelineStatus getStatus();

    /**
     * @brief Enable health monitoring.
     * @param[in] callback Callback for abnormal status notifications.
     * @param[in] userData User context passed back to callback.
     * @param[in] intervalMs Polling interval in milliseconds.
     */
    void enableHealthMonitor(ob_pipeline_status_callback callback, void *userData, uint32_t intervalMs);

    /**
     * @brief Disable health monitoring.
     */
    void disableHealthMonitor();

    /**
     * @brief Reset collector internal state, health monitor will be stop
     * @details Typically called on pipeline start/stop.
     */
    void reset();

    /**
     * @brief Resume health monitor thread if monitor is enabled
     */
    void resume();

    /**
     * @brief Set no-frame detection threshold.
     * @param[in] thresholdUsec Threshold in microseconds.
     */
    void setNoFrameThresholdUsec(uint64_t thresholdUsec);

    /**
     * @brief Set minimum interval for device-status queries.
     * @param[in] intervalMs Interval in milliseconds.
     */
    void setDevFetchIntervalMs(uint64_t intervalMs);

private:
    /**
     * @brief Start health monitor thread.
     */
    void startHealthMonitorThread();

    /**
     * @brief Collect status and reset accumulators.
     * @param[in] forceDeviceQuery If true, always query device/driver status regardless of SDK status.
     * @return Collected pipeline status.
     */
    OBPipelineStatus getStatusAndReset(bool forceDeviceQuery = false);

    /**
     * @brief Evaluate no-frame condition and update status bits.
     * @param[in,out] status Status object to update.
     */
    void evaluateNoFrame(OBPipelineStatus &status);

    /**
     * @brief Determine whether device/driver query is needed.
     * @param[in] sdkStatus Current SDK status bits.
     * @return True if device/driver status should be queried.
     */
    bool needDeviceQuery(uint64_t sdkStatus);

    /**
     * @brief Fetch and fill device and driver status.
     * @param[in,out] status Status object to update.
     * @param[in] forceFetch If true, bypass the time-based cache and fetch fresh data from device.
     */
    void fetchDeviceAndDriverStatus(OBPipelineStatus &status, bool forceFetch = false);

    /**
     * @brief Aggregate driver status from active ports.
     * @return Aggregated driver status bitmask.
     */
    uint64_t fetchPortDriverStatus();

    /**
     * @brief Derive issue category flags from detailed status.
     * @param[in] s Detailed pipeline status.
     * @return Derived issue flags.
     */
    OBPipelineIssue deriveIssue(const OBPipelineStatus &s);

    /**
     * @brief Get current system timestamp in microseconds.
     * @return Current timestamp in microseconds.
     */
    uint64_t nowUsec();

    /**
     * @brief Get current steady clock time point.
     * @return Current steady clock time point.
     */
    std::chrono::steady_clock::time_point steadyNow();

private:
    IDevice *device_;

    std::atomic<uint64_t> sdkStatus_{ 0 };

    std::mutex                       framTimeMutex_;
    std::map<OBStreamType, uint64_t> lastFrameTime_;

    uint64_t noFrameThresholdUsec_ = 3000000;  // 3 seconds

    uint64_t                              cachedDevStatus_ = 0;
    uint64_t                              cachedDrvStatus_ = 0;
    std::chrono::steady_clock::time_point lastDevFetchTime_{};
    std::chrono::milliseconds             devFetchInterval_{ 3000 };

    std::mutex                                portsMutex_;
    std::vector<std::shared_ptr<ISourcePort>> activePorts_;

    // External collector (set by Pipeline to collect sensor drop stats)
    std::function<void()> externalCollector_;

    // Health monitor polling thread
    std::mutex                  monitorMutex_;
    std::condition_variable     monitorCv_;
    std::thread                 monitorThread_;
    ob_pipeline_status_callback monitorCallback_ = nullptr;
    void                       *monitorUserData_ = nullptr;
    uint32_t                    intervalMs_{ 0 };
    bool                        monitorStop_ = true;
    std::atomic<bool>           monitorEnable_{ false };

    OBPipelineStatus        lastStatusCache_{};
    bool                    monitorCollectRequested_ = false;
    uint64_t                monitorCollectReqSeq_    = 0;
    uint64_t                monitorCollectDoneSeq_   = 0;
    std::condition_variable monitorCollectCv_;
};

}  // namespace libobsensor
