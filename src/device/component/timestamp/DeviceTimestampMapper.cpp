// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceTimestampMapper.hpp"
#include "utils/Utils.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace libobsensor {

const double DeviceTimestampMapper::STABLE_ERROR_BUDGET_US = 500.0;

DeviceTimestampMapper::DeviceTimestampMapper(uint64_t deviceTimeFreq) {
    const double freq = deviceTimeFreq > 0 ? static_cast<double>(deviceTimeFreq) : 1000.0;
    ticksPerMs_       = freq / 1000.0;

    // Real-time authored bounds converted to device ticks (1 ms = ticksPerMs_ ticks).
    activationGuardTicks_       = 200.0 * ticksPerMs_;
    minTransitionDurationTicks_ = 1000.0 * ticksPerMs_;
    maxTransitionDurationTicks_ = 2000.0 * ticksPerMs_;
    historyWindowTicks_         = 60000.0 * ticksPerMs_;
    // Rate is us per real ms, so it converts inversely to us per device tick.
    maxPhaseCorrectionRateUsPerTick_ = 1.0 / ticksPerMs_;
}

double DeviceTimestampMapper::evaluateModel(const Model &model, double deviceTimestampTicks) {
    return model.refSteadyUs + model.slopeUsPerTick * (deviceTimestampTicks - model.refDeviceTicks);
}

double DeviceTimestampMapper::estimateCurrentDeviceTicks(const Model &model, double currentSteadyUs) {
    double sampleSteadyUs     = evaluateModel(model, model.sampleDeviceTicks);
    double currentDeviceTicks = model.sampleDeviceTicks + (currentSteadyUs - sampleSteadyUs) / model.slopeUsPerTick;
    if(!std::isfinite(currentDeviceTicks)) {
        return model.sampleDeviceTicks;
    }
    return (std::max)(model.sampleDeviceTicks, currentDeviceTicks);
}

size_t DeviceTimestampMapper::findSegmentLocked(double deviceTimestampTicks) const {
    auto iter = std::upper_bound(timeline_.begin(), timeline_.end(), deviceTimestampTicks,
                                 [](double value, const Segment &segment) { return value < segment.beginDeviceTicks; });
    if(iter == timeline_.begin()) {
        return 0;
    }
    return static_cast<size_t>(std::distance(timeline_.begin(), iter - 1));
}

double DeviceTimestampMapper::evaluateLocked(double deviceTimestampTicks, double *slopeUsPerTick) const {
    const auto &segment = timeline_[findSegmentLocked(deviceTimestampTicks)];
    double      value   = evaluateModel(segment.target, deviceTimestampTicks);
    double      slope   = segment.target.slopeUsPerTick;

    double durationTicks = segment.transitionEndDeviceTicks - segment.beginDeviceTicks;
    if(durationTicks > 0.0 && deviceTimestampTicks < segment.transitionEndDeviceTicks) {
        double u  = (std::max)(0.0, (deviceTimestampTicks - segment.beginDeviceTicks) / durationTicks);
        double u2 = u * u;
        double u3 = u2 * u;

        double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
        double h10 = u3 - 2.0 * u2 + u;
        value += segment.startResidualUs * h00 + durationTicks * segment.startResidualSlopeUsPerTick * h10;

        double dh00 = 6.0 * u2 - 6.0 * u;
        double dh10 = 3.0 * u2 - 4.0 * u + 1.0;
        slope += segment.startResidualUs * dh00 / durationTicks + segment.startResidualSlopeUsPerTick * dh10;
    }

    if(slopeUsPerTick) {
        *slopeUsPerTick = slope;
    }
    return value;
}

void DeviceTimestampMapper::discardFutureSegmentsLocked(double currentDeviceTicks) {
    while(timeline_.size() > 1 && timeline_.back().beginDeviceTicks > currentDeviceTicks) {
        timeline_.pop_back();
    }
}

void DeviceTimestampMapper::pruneHistoryLocked(double currentDeviceTicks) {
    if(timeline_.size() <= 2 || currentDeviceTicks <= historyWindowTicks_) {
        return;
    }

    double cutoffDeviceTicks = currentDeviceTicks - historyWindowTicks_;
    auto   firstAfterCutoff  = std::upper_bound(timeline_.begin(), timeline_.end(), cutoffDeviceTicks,
                                                [](double value, const Segment &segment) { return value < segment.beginDeviceTicks; });
    if(firstAfterCutoff == timeline_.begin()) {
        return;
    }

    // Keep the segment that owns the cutoff plus every newer segment.
    auto firstToKeep = firstAfterCutoff - 1;
    timeline_.erase(timeline_.begin(), firstToKeep);
}

TimestampMapperUpdateInfo DeviceTimestampMapper::appendTransitionLocked(Model target, double currentDeviceTicks) {
    TimestampMapperUpdateInfo info{};
    discardFutureSegmentsLocked(currentDeviceTicks);
    pruneHistoryLocked(currentDeviceTicks);

    target.generation           = ++generation_;
    double transitionStartTicks = std::ceil(currentDeviceTicks + activationGuardTicks_);
    double oldSlopeUsPerTick    = 0.0;
    double oldValueUs           = evaluateLocked(transitionStartTicks, &oldSlopeUsPerTick);
    double newValueUs           = evaluateModel(target, transitionStartTicks);
    double residualUs           = oldValueUs - newValueUs;
    double residualSlope        = oldSlopeUsPerTick - target.slopeUsPerTick;
    // h00'(u) peaks at 1.5, so this duration limits the phase term's extra slope to
    // maxPhaseCorrectionRateUsPerTick_ (i.e. 1 us per real ms) until the 2s (real-time)
    // convergence limit is reached.
    double transitionDurationTicks = 1.5 * std::fabs(residualUs) / maxPhaseCorrectionRateUsPerTick_;
    transitionDurationTicks        = (std::max)(minTransitionDurationTicks_, transitionDurationTicks);
    transitionDurationTicks        = (std::min)(maxTransitionDurationTicks_, transitionDurationTicks);

    Segment transition{};
    transition.beginDeviceTicks            = transitionStartTicks;
    transition.transitionEndDeviceTicks    = transitionStartTicks + transitionDurationTicks;
    transition.target                      = target;
    transition.startResidualUs             = residualUs;
    transition.startResidualSlopeUsPerTick = residualSlope;
    timeline_.push_back(transition);
    latestModel_ = target;

    // Diagnostics are reported in real physical units (ms, us/ms).
    info.generation              = target.generation;
    info.transitionStartDeviceMs = transition.beginDeviceTicks / ticksPerMs_;
    info.transitionEndDeviceMs   = transition.transitionEndDeviceTicks / ticksPerMs_;
    info.phaseCorrectionUs       = residualUs;
    info.slopeCorrectionUsPerMs  = residualSlope * ticksPerMs_;
    info.smoothed                = true;
    return info;
}

TimestampMapperUpdateInfo DeviceTimestampMapper::update(const LinearFuncParam &param, double fitErrorUs) {
    TimestampMapperUpdateInfo info{};
    if(param.coefficientA <= 0.0 || !std::isfinite(param.coefficientA)) {
        return info;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    Model newFit{};
    newFit.slopeUsPerTick    = param.coefficientA;
    newFit.refDeviceTicks    = static_cast<double>(param.refDevTime);
    newFit.refSteadyUs       = static_cast<double>(param.refSysTime);
    newFit.sampleDeviceTicks = static_cast<double>(param.devTime);
    newFit.fitErrorUs        = (std::max)(0.0, fitErrorUs);

    double currentDeviceTicks = 0.0;
    if(valid_) {
        double currentSteadyUs     = static_cast<double>(utils::getSteadyTimeUs());
        currentDeviceTicks         = estimateCurrentDeviceTicks(newFit, currentSteadyUs);
        double previousDeviceTicks = estimateCurrentDeviceTicks(latestModel_, currentSteadyUs);
        if(resetPending_) {
            info.resetHandled       = true;
            info.resetDeviceShiftMs = (currentDeviceTicks - previousDeviceTicks) / ticksPerMs_;
        }
    }

    if(!valid_ || resetPending_) {
        const bool initialized = !valid_;
        newFit.generation      = ++generation_;
        Segment initial{};
        initial.beginDeviceTicks            = 0.0;
        initial.transitionEndDeviceTicks    = 0.0;
        initial.target                      = newFit;
        initial.startResidualUs             = 0.0;
        initial.startResidualSlopeUsPerTick = 0.0;
        timeline_.clear();
        timeline_.push_back(initial);
        latestModel_      = newFit;
        valid_            = true;
        resetPending_     = false;
        info.generation   = newFit.generation;
        info.initialized  = initialized;
        info.resetApplied = info.resetHandled;
        return info;
    }

    resetPending_                 = false;
    auto transition               = appendTransitionLocked(newFit, currentDeviceTicks);
    transition.resetHandled       = info.resetHandled;
    transition.resetDeviceShiftMs = info.resetDeviceShiftMs;
    return transition;
}

GlobalTimestampMapResult DeviceTimestampMapper::map(double deviceTimestampTicks) const {
    GlobalTimestampMapResult result{};
    if(deviceTimestampTicks <= 0.0 || !std::isfinite(deviceTimestampTicks)) {
        return result;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if(!valid_ || timeline_.empty()) {
        return result;
    }

    double mappedUs = evaluateLocked(deviceTimestampTicks);
    if(mappedUs < 0.0 || mappedUs > static_cast<double>((std::numeric_limits<uint64_t>::max)())) {
        return result;
    }

    double targetUs              = evaluateModel(latestModel_, deviceTimestampTicks);
    double remainingUs           = mappedUs - targetUs;
    double estimatedUs           = std::fabs(remainingUs) + latestModel_.fitErrorUs;
    result.timestampUs           = static_cast<uint64_t>(mappedUs + 0.5);
    result.generation            = latestModel_.generation;
    result.remainingCorrectionUs = remainingUs;
    result.estimatedErrorUs      = estimatedUs;
    result.valid                 = true;
    result.stable                = estimatedUs <= STABLE_ERROR_BUDGET_US;
    return result;
}

void DeviceTimestampMapper::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Keep the last valid mapping while the fitter collects new samples.
    if(valid_ && !timeline_.empty()) {
        resetPending_ = true;
        ++generation_;
        return;
    }

    timeline_.clear();
    latestModel_  = Model{};
    valid_        = false;
    resetPending_ = false;
    ++generation_;
}

}  // namespace libobsensor
