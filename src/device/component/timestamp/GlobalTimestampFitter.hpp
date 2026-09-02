// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"
#include "DeviceComponentBase.hpp"
#include "DeviceTimestampMapper.hpp"
#include "utils/SteadyCondVar.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace libobsensor {

typedef struct {
    uint64_t time;        ///< Estimated system timestamp (us)
    uint64_t rtt;         ///< Raw round-trip time (us)
    uint64_t rttRef;      ///< Filtered RTT used for estimation (us)
    bool     rttOutlier;  ///< Raw RTT is outside the robust RTT-window limit
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

            // MAD: median absolute deviation - robust spread estimate
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
                return { t2 - rttRef / 2, rtt, rttRef, true };
            }
        }

        // Normal sample: add to window
        window_.push_back(rtt);
        if(window_.size() > WINDOW_SIZE) {
            window_.pop_front();
        }

        return { t2 - rttRef / 2, rtt, rttRef, false };
    }

private:
    const uint32_t WINDOW_SIZE     = 50;
    const uint64_t SPREAD_FLOOR_US = 200;  // absolute lower bound for the adaptive floor
    const uint64_t SPREAD_K        = 4;    // multiplier: threshold = median + K * max(MAD, floor)

    std::deque<uint64_t> window_;
};

class GlobalTimestampFitter : public IGlobalTimestampFitter, public DeviceComponentBase {
public:
    GlobalTimestampFitter(IDevice *owner, uint64_t deviceTimeFreq = 1000);
    virtual ~GlobalTimestampFitter() override;

    LinearFuncParam          getLinearFuncParam() override;
    GlobalTimestampMapResult mapDeviceTime(double deviceTimestampTicks) override;
    void                     reFitting(bool async) override;
    void                     pause() override;
    void                     resume() override;
    void                     setMaxValidRtt(uint64_t maxValidTime);

    void enable(bool en) override;
    bool isEnabled() const override;

private:
    enum class UpdateResult {
        Rejected,
        NotMature,
        Ready,
    };

    void                      fittingLoop();
    inline const std::string &GetCurrentSN() const;
    void                      calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp);
    void                      ensureFitting();
    double                    getSampleNoiseScale(const OBTimeSample &timeSample) const;

    /**
     * @brief Acquire one (sysTimestamp, devTime) sample; false on RTT overflow or exception.
     */
    bool acquireSample(OBTimeSample &timeSample, OBDeviceTime &devTime);
    /**
     * @brief Update samplingQueue_ (drift reverify, out-of-order reset, size cap).
     * @return Rejected when no usable sample was retained; NotMature when a sample was retained but fewer than four samples are available;
     *         Ready when a refit can run.
     */
    UpdateResult updateSampleQueue(const OBTimeSample &timeSample, const OBDeviceTime &devTime);

private:
    struct CallbackContext {
        std::mutex             mutex;
        GlobalTimestampFitter *fitter = nullptr;
    };

    const uint64_t MAX_VALID_RTT = 20000;  // 20ms

    // Timestamp discontinuity is confirmed when (trigger + agreeing reverifies) reach DRIFT_MIN_AGREE;
    // queue is then rebuilt from new samples only. This is for abrupt retiming/jumps, not slow clock drift.
    const uint64_t RESIDUAL_JUMP_THRESHOLD_US = 20000;  // 20ms
    // A single RTT outlier must not move the mapping when its timestamp disagrees with
    // the latest accepted mapping. Use the normal RTT reference (rather than the raw
    // outlier RTT) so a delayed request cannot enlarge its own acceptance window.
    const uint64_t RTT_OUTLIER_RESIDUAL_MIN_US = 3000;  // 3ms
    const uint64_t RTT_OUTLIER_RESIDUAL_RTT_K  = 5;
    const uint32_t REVERIFY_SAMPLES            = 5;
    const uint32_t REVERIFY_INTERVAL_MS        = 500;  // 5 samples span 2.5s for slope estimation
    const size_t   DRIFT_MIN_AGREE             = 4;    // out of (1 trigger + 5 reverify) = 6
    // Queue size considered "mature": gates the fitting cadence and drift-jump detection.
    const size_t MATURE_QUEUE_SIZE = 15;

    // Recursive clock-estimator (Kalman) parameters. State x = [O, S]: host-steady
    // offset (us) and clock skew (us/tick). The offset is seeded from a weighted mean
    // over the first KF_SEED_SAMPLES samples and the skew from the nominal device clock
    // rate, then both are advanced per sample. See calcLinearParam().
    // All noise parameters below are authored for the 1 kHz axis (1 tick == 1 ms) and
    // scaled by ticksPerMs (deviceTimeFreq_/1000) in calcLinearParam() so their physical
    // meaning is identical across device clock frequencies.
    const size_t KF_SEED_SAMPLES = 6;
    // Adaptive skew process noise: q_s = clamp(BASE * nisAvg, MIN, MAX), where
    // nisAvg is an EWMA of the normalized innovation squared (innov^2 / Ri). A stable
    // clock keeps innovations near the measurement floor (nisAvg ~ 1) -> low q_s ->
    // heavy smoothing; a wandering clock inflates innovations -> high q_s -> tracking.
    const double KF_PROCESS_NOISE_BASE          = 1.0e-5;
    const double KF_PROCESS_NOISE_MIN           = 1.0e-6;
    const double KF_PROCESS_NOISE_MAX           = 2.0e-3;
    const double KF_NIS_ADAPT_BETA              = 0.1;    // EWMA weight for nisAvg
    const double KF_MEAS_NOISE_US2              = 1.0e6;  // R: RTT latch measurement variance, (1000us)^2
    const double KF_RTT_OUTLIER_NOISE_SCALE_MAX = 64.0;
    const double KF_OFFSET_NOISE_RATIO          = 1.0e-3;  // q_o = ratio * R: small offset process noise
    // Seed the skew from the nominal device-to-host rate (host steady us per device
    // clock tick: 1e6 / deviceTimeFreq_ -> 1000 us/tick for 1 kHz devices, 1.0 us/tick
    // for 1 MHz devices) instead of a short-baseline least-squares slope, which is
    // unreliable at startup. The ppm skew is refined online.
    const double KF_SEED_SLOPE_STD = 0.05;  // initial skew std at 1 kHz (us/tick); scaled by 1/ticksPerMs -> ~50ppm on every clock

    bool                 enable_;
    std::thread          sampleThread_;
    std::mutex           sampleMutex_;
    utils::SteadyCondVar sampleCondVar_;
    std::atomic<bool>    sampleLoopExit_;
    std::atomic<bool>    needBootstrap_{ false };
    // Serializes acquire+queue+fit between fittingLoop and ensureFitting to avoid reFitting()/background races.
    std::mutex samplingOpMutex_;

    typedef struct {
        uint64_t systemTimestamp;
        uint64_t deviceTimestamp;
        double   measurementNoiseScale;
    } TimestampPair;

    std::deque<TimestampPair> samplingQueue_;
    uint32_t                  maxQueueSize_{ 100 };
    bool                      needCalculation_{ false };  // true if samplingQueue_ changed

    // The refresh interval needs to be less than half the interval of the data frame, that is, it needs to be sampled at least twice within an overflow period.
    uint32_t refreshIntervalMsec_ = 1000;

    std::mutex                       linearFuncParamMutex_;
    utils::SteadyCondVar             linearFuncParamCondVar_;
    LinearFuncParam                  linearFuncParam_;
    DeviceTimestampMapper            timestampMapper_;
    uint64_t                         lastSysTime_ = 0;
    uint64_t                         maxValidRtt_;
    uint64_t                         deviceTimeFreq_;  // Device clock frequency (Hz): 1000 (ms) or 1000000 (us) per device family
    RttWindow                        rttWindow_;
    std::weak_ptr<IPropertyServer>   propertyServer_;
    std::shared_ptr<CallbackContext> callbackContext_;
    uint64_t                         resetCallbackToken_ = 0;
    uint64_t                         ptpCallbackToken_   = 0;
};
}  // namespace libobsensor
