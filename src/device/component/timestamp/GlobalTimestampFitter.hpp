// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
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
    explicit RttWindow(uint32_t windowSize = 20, uint64_t spreadFloor = 200, uint64_t spreadK = 4)
        : windowSize_(windowSize), spreadFloor_(spreadFloor), spreadK_(spreadK) {}

    void updateSpreadParams(uint64_t floor, uint64_t k) {
        spreadFloor_ = floor;
        spreadK_     = k;
    }

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

            // threshold = median + K * max(MAD, floor)
            // floor prevents over-rejection when the distribution is very tight
            uint64_t threshold = median + spreadK_ * (std::max)(mad, spreadFloor_);
            if(rtt > threshold) {
                // Outlier: use window median as rttRef, do NOT add to window
                rttRef = median;
                return { t1 + rttRef / 2, rtt, rttRef };
            }
        }

        // Normal sample: add to window
        window_.push_back(rtt);
        if(window_.size() > windowSize_) {
            window_.pop_front();
        }

        return { t1 + rttRef / 2, rtt, rttRef };
    }

private:
    uint32_t             windowSize_;
    uint64_t             spreadFloor_;  // µs, minimum MAD to avoid over-rejection
    uint64_t             spreadK_;      // multiplier: threshold = median + K * max(MAD, floor)
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

private:
    void                      fittingLoop();
    inline const std::string &GetCurrentSN() const;
    void                      calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp);
    void                      ensureFitting();

private:
    const uint64_t MAX_VALID_RTT = 20000;  // 10ms

    bool                    enable_;
    std::thread             sampleThread_;
    std::mutex              sampleMutex_;
    std::condition_variable sampleCondVar_;
    std::atomic<bool>       sampleLoopExit_;

    typedef struct {
        uint64_t systemTimestamp;
        uint64_t deviceTimestamp;
    } TimestampPair;

    std::deque<TimestampPair> samplingQueue_;
    uint32_t                  maxQueueSize_{ 100 };
    double                    decayHalfLifeSec_{ 180.0 };  // EWLR half-life in seconds; 0 = unweighted
    bool                      needCalculation_{ false };   // true if samplingQueue_ changed

    // The refresh interval needs to be less than half the interval of the data frame, that is, it needs to be sampled at least twice within an overflow period.
    uint32_t refreshIntervalMsec_ = 1000;

    std::mutex              linearFuncParamMutex_;
    std::condition_variable linearFuncParamCondVar_;
    LinearFuncParam         linearFuncParam_;
    uint64_t                lastCheckDataY = 0;
    uint64_t                maxValidRtt_;
    RttWindow               rttWindow_;
};
}  // namespace libobsensor
