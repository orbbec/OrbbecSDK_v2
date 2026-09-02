// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrameTimestamp.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace libobsensor {

struct TimestampMapperUpdateInfo {
    uint64_t generation;
    double   transitionStartDeviceMs;
    double   transitionEndDeviceMs;
    double   phaseCorrectionUs;
    double   slopeCorrectionUsPerMs;
    double   resetDeviceShiftMs;
    bool     initialized;
    bool     smoothed;
    bool     resetHandled;
    bool     resetApplied;
};

/**
 * @brief Device-scoped, deterministic device-to-steady timestamp mapper.
 *
 * Fitted updates take effect at a future device timestamp and are represented
 * as immutable timeline segments. Every stream sharing a device maps the same
 * device timestamp through the same segment. Recent history is retained for
 * delayed frames while older, unreachable segments are bounded.
 *
 * The internal timeline axis is device clock ticks (host_us = slope * ticks).
 * Physical configuration (transition/history bounds, phase-correction rate) is
 * authored in real time and converted to ticks via ticksPerMs_ at construction,
 * so behavior is identical across device clock frequencies. Diagnostic fields in
 * TimestampMapperUpdateInfo are converted back to real ms / us-per-ms.
 */
class DeviceTimestampMapper {
public:
    // deviceTimeFreq is the device clock frequency (Hz). One real millisecond spans
    // deviceTimeFreq/1000 device ticks, so the real-time bounds below are scaled to
    // ticks at construction.
    explicit DeviceTimestampMapper(uint64_t deviceTimeFreq = 1000);

    TimestampMapperUpdateInfo update(const LinearFuncParam &param, double fitErrorUs);
    GlobalTimestampMapResult  map(double deviceTimestampTicks) const;
    void                      reset();

private:
    struct Model {
        double   slopeUsPerTick;
        double   refDeviceTicks;
        double   refSteadyUs;
        double   sampleDeviceTicks;
        double   fitErrorUs;
        uint64_t generation;
    };

    struct Segment {
        double beginDeviceTicks;
        double transitionEndDeviceTicks;
        Model  target;
        double startResidualUs;
        double startResidualSlopeUsPerTick;
    };

    static double evaluateModel(const Model &model, double deviceTimestampTicks);
    static double estimateCurrentDeviceTicks(const Model &model, double currentSteadyUs);

    TimestampMapperUpdateInfo appendTransitionLocked(Model target, double currentDeviceTicks);
    size_t                    findSegmentLocked(double deviceTimestampTicks) const;
    double                    evaluateLocked(double deviceTimestampTicks, double *slopeUsPerTick = nullptr) const;
    void                      discardFutureSegmentsLocked(double currentDeviceTicks);
    void                      pruneHistoryLocked(double currentDeviceTicks);

private:
    // deviceTimeFreq/1000: device ticks per real millisecond. The bounds below are
    // authored in real time and converted to ticks in the constructor so the mapper
    // behaves identically across device clock frequencies (e.g. 1 kHz vs 1 MHz device
    // timestamps); a fixed us-per-ms rate would otherwise let phase corrections drive
    // the mapped slope to zero on high-frequency clocks.
    double ticksPerMs_;
    double activationGuardTicks_;
    // Phase corrections use a Hermite transition. Bound the transition to keep
    // every fitted model effective within a predictable time while limiting the
    // additional timestamp-rate change caused by large phase corrections.
    double minTransitionDurationTicks_;
    double maxTransitionDurationTicks_;
    double maxPhaseCorrectionRateUsPerTick_;
    double historyWindowTicks_;
    // Host-us error budget: independent of the device clock frequency, no scaling.
    static const double STABLE_ERROR_BUDGET_US;

    mutable std::mutex   mutex_;
    std::vector<Segment> timeline_;
    Model                latestModel_{};
    uint64_t             generation_   = 0;
    bool                 valid_        = false;
    bool                 resetPending_ = false;
};

}  // namespace libobsensor
