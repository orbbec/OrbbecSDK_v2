// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"
#include "utils/SteadyCondVar.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <limits>
#include <atomic>

namespace libobsensor {

typedef struct {
    uint64_t time;    ///< Estimated system timestamp (us)
    uint64_t rtt;     ///< Raw round-trip time (us)
    uint64_t rttRef;  ///< Filtered RTT used for estimation (us)
} OBTimeSample;

// RttWindow: sliding window of "clean" RTT samples for timestamp estimation.
// Outliers (detected via median + spread) are rejected and not added to the window.
// When an outlier is detected the window median is used as rttRef instead.
class RttWindow {
public:
    RttWindow() = default;

    void clear() {
        window_.clear();
    }

    // Returns estimated latch timestamp given t1 (before XU call) and t2 (after XU call).
    OBTimeSample estimate(uint64_t t1, uint64_t t2) {
        uint64_t rtt    = t2 - t1;
        uint64_t rttRef = rtt;

        if(window_.size() >= 4) {
            std::vector<uint64_t> s(window_.begin(), window_.end());
            std::sort(s.begin(), s.end());

            uint64_t median = s[s.size() / 2];

            // MAD: median absolute deviation — robust spread estimate
            std::vector<uint64_t> devs;
            devs.reserve(s.size());
            for(auto v: s) {
                devs.push_back(v >= median ? v - median : median - v);
            }
            std::sort(devs.begin(), devs.end());
            uint64_t mad = devs[devs.size() / 2];

            // Adaptive floor: scales with median so the tolerance band naturally tracks RTT
            // magnitude across links (GMSL/USB/100M/1G ethernet) without per-transport tuning.
            // SPREAD_FLOOR_US is the absolute lower bound to prevent over-rejection on very fast links.
            uint64_t floorEff  = (std::max)(SPREAD_FLOOR_US, median / 5);
            uint64_t threshold = median + SPREAD_K * (std::max)(mad, floorEff);
            if(rtt > threshold) {
                // Outlier: use window median as rttRef, do NOT add to window
                rttRef = median;
                // Anchor to t2: equivalent to t1+rttRef/2 on normal path, but more accurate
                // when rtt > rttRef since the extra delay is usually pre-send scheduling jitter.
                return { t2 - rttRef / 2, rtt, rttRef };
            }
        }

        // Normal sample: add to window
        window_.push_back(rtt);
        if(window_.size() > WINDOW_SIZE) {
            window_.pop_front();
        }

        return { t2 - rttRef / 2, rtt, rttRef };
    }

private:
    const uint32_t WINDOW_SIZE     = 50;
    const uint64_t SPREAD_FLOOR_US = 200;  // absolute lower bound for the adaptive floor
    const uint64_t SPREAD_K        = 4;    // multiplier: threshold = median + K * max(MAD, floor)

    std::deque<uint64_t> window_;
};

class GlobalTimestampFitter : public IGlobalTimestampFitter, public DeviceComponentBase {
public:
    GlobalTimestampFitter(IDevice *owner);
    virtual ~GlobalTimestampFitter() override;

    LinearFuncParam getLinearFuncParam() override;
    void            reFitting(bool async) override;
    void            pause() override;
    void            resume() override;
    void            setMaxValidRtt(uint64_t maxValidTime);

    void enable(bool en) override;
    bool isEnabled() const override;
    bool isPtpActive() const override;

private:
    void                      fittingLoop();
    inline const std::string &GetCurrentSN() const;
    void                      calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp);
    void                      ensureFitting();

    /**
     * @brief Acquire one (sysTimestamp, devTime) sample; false on RTT overflow or exception.
     */
    bool acquireSample(OBTimeSample &timeSample, OBDeviceTime &devTime);
    /**
     * @brief Update samplingQueue_ (drift reverify, out-of-order reset, size cap); true if refit needed.
     */
    bool updateSampleQueue(const OBTimeSample &timeSample, const OBDeviceTime &devTime);

private:
    const uint64_t MAX_VALID_RTT = 20000;  // 10ms

    // Timestamp discontinuity is confirmed when (trigger + agreeing reverifies) reach DRIFT_MIN_AGREE;
    // queue is then rebuilt from new samples only. This is for abrupt retiming/jumps, not slow clock drift.
    const uint64_t RESIDUAL_JUMP_THRESHOLD_US = 20000;  // 20ms
    const uint32_t REVERIFY_SAMPLES           = 5;
    const uint32_t REVERIFY_INTERVAL_MS       = 500;  // 5 samples span 2.5s for slope estimation
    const size_t   DRIFT_MIN_AGREE            = 4;    // out of (1 trigger + 5 reverify) = 6
    // Queue size considered "mature": gates EWLR weighting and drift-jump detection.
    const size_t MATURE_QUEUE_SIZE = 15;

    bool                 enable_;
    std::thread          sampleThread_;
    std::mutex           sampleMutex_;
    utils::SteadyCondVar sampleCondVar_;
    std::atomic<bool>    sampleLoopExit_;
    std::atomic<bool>    needBootstrap_{ false };
    std::atomic<bool>    ptpActive_{ false };
    // Serializes acquire+queue+fit between fittingLoop and ensureFitting to avoid reFitting()/background races.
    std::mutex samplingOpMutex_;

    typedef struct {
        uint64_t systemTimestamp;
        uint64_t deviceTimestamp;
    } TimestampPair;

    std::deque<TimestampPair> samplingQueue_;
    uint32_t                  maxQueueSize_{ 100 };
    double                    decayHalfLifeSec_{ 600.0 };  // EWLR half-life in seconds; 0 = unweighted
    bool                      needCalculation_{ false };   // true if samplingQueue_ changed

    // The refresh interval needs to be less than half the interval of the data frame, that is, it needs to be sampled at least twice within an overflow period.
    uint32_t refreshIntervalMsec_ = 1000;

    std::mutex           linearFuncParamMutex_;
    utils::SteadyCondVar linearFuncParamCondVar_;
    LinearFuncParam      linearFuncParam_;
    uint64_t             lastSysTime_ = 0;
    uint64_t             maxValidRtt_;
    RttWindow            rttWindow_;
};
}  // namespace libobsensor
