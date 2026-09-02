// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GlobalTimestampFitter.hpp"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "environment/EnvConfig.hpp"

#include <cmath>
#include <utility>

#include "logger/LoggerSnWrapper.hpp"  // Must be included last to override log macros

namespace libobsensor {

const std::string &GlobalTimestampFitter::GetCurrentSN() const {
    auto owner = getOwner();
    if(owner) {
        return owner->getSn();
    }

    static std::string unknown = "Unknown";
    return unknown;
}

GlobalTimestampFitter::GlobalTimestampFitter(IDevice *owner, uint64_t deviceTimeFreq)
    : DeviceComponentBase(owner),
      enable_(false),
      sampleLoopExit_(false),
      linearFuncParam_({ 0, 0, 0, 0, 0 }),
      timestampMapper_(deviceTimeFreq > 0 ? deviceTimeFreq : 1000),
      maxValidRtt_(MAX_VALID_RTT),
      deviceTimeFreq_(deviceTimeFreq > 0 ? deviceTimeFreq : 1000),
      callbackContext_(std::make_shared<CallbackContext>()) {
    // RttWindow uses an adaptive floor that scales with the observed RTT median,
    // so no per-transport (USB/Ethernet/GMSL) tuning is needed here.

    // config
    std::string deviceName = utils::string::removeSpace(owner->getInfo()->name_);
    auto        envConfig  = EnvConfig::getInstance();
    int         value      = 0;
    std::string key        = std::string("Device.") + deviceName + std::string(".Misc.GlobalTimestampFitterQueueSize");
    if(envConfig->getIntValue(key, value) && value >= 4) {
        maxQueueSize_ = value;
    }
    value = 0;
    key   = std::string("Device.") + deviceName + std::string(".Misc.GlobalTimestampFitterInterval");
    if(envConfig->getIntValue(key, value) && value >= 100) {
        refreshIntervalMsec_ = value;
    }

    bool en = false;
    key     = std::string("Device.") + deviceName + std::string(".Misc.GlobalTimestampFitterEnable");
    if(envConfig->getBooleanValue(key, en)) {
        enable(en);
    }

    auto propServer          = owner->getPropertyServer();
    propertyServer_          = propServer.get();
    callbackContext_->fitter = this;
    if(propServer->isPropertySupported(OB_PROP_TIMER_RESET_SIGNAL_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        auto callbackContext = callbackContext_;
        resetCallbackToken_  = propServer->registerAccessCallback(OB_PROP_TIMER_RESET_SIGNAL_BOOL,
                                                                  [callbackContext](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
                                                                     if(operationType == PROP_OP_WRITE) {
                                                                         std::lock_guard<std::mutex> lock(callbackContext->mutex);
                                                                         if(callbackContext->fitter != nullptr) {
                                                                             callbackContext->fitter->reFitting(false);
                                                                         }
                                                                     }
                                                                 });
    }
    // PTP clock sync enable
    if(propServer->isPropertySupported(OB_DEVICE_PTP_CLOCK_SYNC_ENABLE_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        auto                   callbackContext = callbackContext_;
        PropertyAccessCallback ptpCallback     = [callbackContext](uint32_t, const uint8_t *data, size_t dataSize, PropertyOperationType operationType) {
            if(operationType != PROP_OP_WRITE || data == nullptr || dataSize < sizeof(OBPropertyValue)) {
                return;
            }

            std::lock_guard<std::mutex> lock(callbackContext->mutex);
            if(callbackContext->fitter != nullptr) {
                // Enabling PTP performs a host timer sync even when the property was
                // already enabled, so every successful write starts a fresh epoch.
                callbackContext->fitter->reFitting(false);
            }
        };
        ptpCallbackToken_ = propServer->registerAccessCallback(OB_DEVICE_PTP_CLOCK_SYNC_ENABLE_BOOL, std::move(ptpCallback));
    }

    LOG_DEBUG("GlobalTimestampFitter created: maxQueueSize_={}, refreshIntervalMsec_={}", maxQueueSize_, refreshIntervalMsec_);
}

GlobalTimestampFitter::~GlobalTimestampFitter() {
    {
        // PropertyServer invokes a snapshot of callbacks outside its own lock.
        // Invalidate this context first so a callback copied before unregister
        // cannot dereference a destroyed fitter.
        std::lock_guard<std::mutex> lock(callbackContext_->mutex);
        callbackContext_->fitter = nullptr;
    }
    if(auto propServer = propertyServer_.lock()) {
        propServer->unregisterAccessCallback(resetCallbackToken_);
        propServer->unregisterAccessCallback(ptpCallbackToken_);
    }

    sampleLoopExit_ = true;
    sampleCondVar_.notify_one();
    if(sampleThread_.joinable()) {
        sampleThread_.join();
    }
    rttWindow_.clear();
}

LinearFuncParam GlobalTimestampFitter::getLinearFuncParam() {
    std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
    if(lastSysTime_ != linearFuncParam_.sysTime) {
        lastSysTime_            = linearFuncParam_.sysTime;
        auto        &param      = linearFuncParam_;
        const double ticksPerMs = static_cast<double>(deviceTimeFreq_) / 1000.0;
        LOG_DEBUG("GetLinearFuncParam: A={:.3f}us/ms, ref=({}tick,{}us), cur=({}tick,{}us)", param.coefficientA * ticksPerMs, param.refDevTime,
                  param.refSysTime, param.devTime, param.sysTime);
    }

    return linearFuncParam_;
}

GlobalTimestampMapResult GlobalTimestampFitter::mapDeviceTime(double deviceTimestampTicks) {
    return timestampMapper_.map(deviceTimestampTicks);
}

void GlobalTimestampFitter::reFitting(bool async) {
    if(!enable_) {
        return;
    }

    {
        // Exclude an in-flight sample/fit before resetting the mapping epoch;
        // otherwise an old-regime fit could be published after the reset.
        std::lock_guard<std::mutex> opLock(samplingOpMutex_);
        timestampMapper_.reset();

        std::unique_lock<std::mutex> sampleLock(sampleMutex_);
        needCalculation_ = true;
        samplingQueue_.clear();
        // Do NOT notify here: avoid fittingLoop racing with ensureFitting during bootstrap.
    }

    if(!async) {
        ensureFitting();
        needBootstrap_ = true;
        sampleCondVar_.notify_one();
    }
    else {
        // No one does bootstrap on the calling thread; wake fittingLoop to do it.
        needBootstrap_ = true;
        sampleCondVar_.notify_one();
    }
}

void GlobalTimestampFitter::pause() {
    sampleLoopExit_ = true;
    sampleCondVar_.notify_one();
    if(sampleThread_.joinable()) {
        sampleThread_.join();
    }
}

void GlobalTimestampFitter::resume() {
    if(enable_) {
        sampleLoopExit_ = false;
        sampleThread_   = std::thread(&GlobalTimestampFitter::fittingLoop, this);
    }
}

void GlobalTimestampFitter::setMaxValidRtt(uint64_t maxValidTime) {
    maxValidRtt_ = maxValidTime;
}

void GlobalTimestampFitter::enable(bool en) {
    if(en == enable_) {
        return;
    }
    enable_ = en;
    if(enable_) {
        sampleLoopExit_ = false;
        sampleThread_   = std::thread(&GlobalTimestampFitter::fittingLoop, this);
        std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
        linearFuncParamCondVar_.wait_for(lock, std::chrono::milliseconds(1000));
    }
    else {
        sampleLoopExit_ = true;
        sampleCondVar_.notify_one();
        if(sampleThread_.joinable()) {
            sampleThread_.join();
        }
        {
            std::unique_lock<std::mutex> lock(sampleMutex_);
            samplingQueue_.clear();
            rttWindow_.clear();
        }
        {
            std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
            linearFuncParam_ = { 0, 0, 0, 0, 0 };
        }
        timestampMapper_.reset();
    }
    LOG_DEBUG("GlobalTimestampFitter@{} enable state changed: {}", reinterpret_cast<uint64_t>(this), enable_);
}

bool GlobalTimestampFitter::isEnabled() const {
    return enable_;
}

void GlobalTimestampFitter::calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp) {
    double kfSlope       = 0.0;  // clock skew S (us/tick)
    double kfRefDevTicks = 0.0;  // absolute device time at the rolling reference (ticks)
    double kfRefSysUs    = 0.0;  // absolute host-steady time at the rolling reference
    size_t queueSize     = 0;
    double residualRmsUs = 0.0;
    // Diagnostics for the update log (adaptive state and innovation extremes over the batch).
    double kfNisAvg        = 1.0;
    double kfQsMin         = 0.0;
    double kfQsMax         = 0.0;
    double kfMaxInnovUs    = 0.0;
    double kfLastInnovUs   = 0.0;
    double kfMaxNoiseScale = 1.0;

    {
        std::unique_lock<std::mutex> lock(sampleMutex_);

        if(!needCalculation_) {
            return;
        }

        queueSize = samplingQueue_.size();
        if(queueSize < 2) {
            return;
        }

        // Recursive clock estimator over the current sample window. State x = [O, S]:
        // O = host-steady offset (us) at the rolling reference device time, S = clock
        // skew (us/tick). The predict step advances O by S*dt and lets S random-walk,
        // so the estimator tracks a curving device<->host relationship (frequency
        // wander) that a fixed-window linear fit cannot follow; each RTT sample is an
        // unbiased measurement of O. Relative coordinates (subtract the oldest sample)
        // preserve precision when converting ~1e15 timestamps to double.
        const double offsetX = (double)samplingQueue_.front().deviceTimestamp;
        const double offsetY = (double)samplingQueue_.front().systemTimestamp;
        const size_t seedN   = (std::min)(KF_SEED_SAMPLES, queueSize);

        // Seed the offset from a plain mean over the first seedN samples; seed the
        // skew from the nominal device-to-host rate (host steady us per device tick)
        // rather than a short-baseline least-squares slope, which over the ~300ms
        // bootstrap window is grossly unreliable and throws off early frames.
        // This runs whenever the window is small -- both at startup and after a queue
        // rebuild (drift/retiming refit) -- so both cases get a clean seed. The ppm
        // skew is refined online and is fully data-driven once the window fills.
        double sx = 0.0, sy = 0.0, seedWeight = 0.0;
        for(size_t i = 0; i < seedN; ++i) {
            const double weight = 1.0 / samplingQueue_[i].measurementNoiseScale;
            sx += weight * ((double)samplingQueue_[i].deviceTimestamp - offsetX);
            sy += weight * ((double)samplingQueue_[i].systemTimestamp - offsetY);
            seedWeight += weight;
            kfMaxNoiseScale = (std::max)(kfMaxNoiseScale, samplingQueue_[i].measurementNoiseScale);
        }
        const double mx   = sx / seedWeight;
        const double my   = sy / seedWeight;
        double       S    = 1.0e6 / static_cast<double>(deviceTimeFreq_);
        double       refX = (double)samplingQueue_[seedN - 1].deviceTimestamp - offsetX;
        double       O    = my + S * (refX - mx);

        // Kalman noise parameters are authored for the 1 kHz axis (1 tick == 1 ms).
        // Scale them by ticksPerMs so their physical meaning (ppm skew std, us/ms
        // offset/skew random walk) is identical on every device clock frequency,
        // without changing the tick-domain filter math. Powers follow the tick<->ms
        // unit change: seed skew std ~ 1/ticksPerMs, offset process noise ~ 1/ticksPerMs,
        // skew process-noise density ~ 1/ticksPerMs^3.
        const double ticksPerMs   = static_cast<double>(deviceTimeFreq_) / 1000.0;
        const double seedSlopeStd = KF_SEED_SLOPE_STD / ticksPerMs;
        const double qsScale      = 1.0 / (ticksPerMs * ticksPerMs * ticksPerMs);
        const double qsBase       = KF_PROCESS_NOISE_BASE * qsScale;
        const double qsMinScaled  = KF_PROCESS_NOISE_MIN * qsScale;
        const double qsMaxScaled  = KF_PROCESS_NOISE_MAX * qsScale;

        // 2x2 state covariance and process/measurement noise. The skew starts with a
        // tight covariance (trust the nominal rate) so bootstrap-window noise cannot
        // corrupt it; it then relaxes via the process noise as real samples arrive.
        double       P00 = KF_MEAS_NOISE_US2;
        double       P01 = 0.0;
        double       P10 = 0.0;
        double       P11 = seedSlopeStd * seedSlopeStd;
        const double R   = KF_MEAS_NOISE_US2;
        const double qo  = KF_MEAS_NOISE_US2 * KF_OFFSET_NOISE_RATIO / ticksPerMs;

        double ssInnov          = 0.0;
        double innovationWeight = 0.0;
        size_t nInnov           = 0;
        double nisAvg           = 1.0;  // EWMA of innov^2/Ri; starts at the stable-clock target
        double qsMin            = qsMaxScaled;
        double qsMax            = qsMinScaled;
        double maxAbsInnov      = 0.0;
        double lastInnov        = 0.0;
        for(size_t i = seedN; i < queueSize; ++i) {
            double d  = (double)samplingQueue_[i].deviceTimestamp - offsetX;
            double z  = (double)samplingQueue_[i].systemTimestamp - offsetY;
            double dd = d - refX;

            // Adaptive skew process noise from the recent innovation level: small when
            // the clock is stable (smooth), large when it wanders (tracks).
            double qs = qsBase * nisAvg;
            qs        = (std::max)(qsMinScaled, (std::min)(qsMaxScaled, qs));
            qsMin     = (std::min)(qsMin, qs);
            qsMax     = (std::max)(qsMax, qs);

            // Predict: advance offset by skew, propagate covariance, add process noise.
            O += S * dd;
            double pP00 = P00 + dd * (P10 + P01) + dd * dd * P11 + qs * dd * dd * dd / 3.0 + qo * std::fabs(dd);
            double pP01 = P01 + dd * P11 + qs * dd * dd / 2.0;
            double pP10 = P10 + dd * P11 + qs * dd * dd / 2.0;
            double pP11 = P11 + qs * std::fabs(dd);
            P00         = pP00;
            P01         = pP01;
            P10         = pP10;
            P11         = pP11;
            refX        = d;

            // Update: the RTT sample measures O (H = [1, 0]).
            double innov    = z - O;
            lastInnov       = innov;
            maxAbsInnov     = (std::max)(maxAbsInnov, std::fabs(innov));
            const double Ri = R * samplingQueue_[i].measurementNoiseScale;
            double       Sk = P00 + Ri;
            double       K0 = P00 / Sk;
            double       K1 = P10 / Sk;
            O += K0 * innov;
            S += K1 * innov;
            double uP00 = (1.0 - K0) * P00;
            double uP01 = (1.0 - K0) * P01;
            double uP10 = P10 - K1 * P00;
            double uP11 = P11 - K1 * P01;
            P00         = uP00;
            P01         = uP01;
            P10         = uP10;
            P11         = uP11;

            nisAvg += KF_NIS_ADAPT_BETA * (innov * innov / Ri - nisAvg);
            kfMaxNoiseScale     = (std::max)(kfMaxNoiseScale, samplingQueue_[i].measurementNoiseScale);
            const double weight = 1.0 / samplingQueue_[i].measurementNoiseScale;
            ssInnov += weight * innov * innov;
            innovationWeight += weight;
            ++nInnov;
        }

        if(!std::isfinite(S) || !std::isfinite(O) || S <= 0.0) {
            LOG_DEBUG("Kalman fit invalid! QueueSize: {}, S={}us/tick", queueSize, S);
            return;
        }
        needCalculation_ = false;

        kfSlope       = S;
        kfRefDevTicks = offsetX + refX;
        kfRefSysUs    = offsetY + O;
        // Innovation RMS (one-step prediction residual) is the fit-quality metric the
        // mapper consumes; before any update is available fall back to sqrt(R).
        residualRmsUs = (nInnov > 0) ? std::sqrt(ssInnov / innovationWeight) : std::sqrt(R);
        kfNisAvg      = nisAvg;
        // Report the process noise in its physical (frequency-independent) scale. When
        // the update loop did not run (window <= seed) qsMin/qsMax keep their reversed
        // init, so leave the reported range at its 0 default in that case.
        if(nInnov > 0) {
            kfQsMin = qsMin / qsScale;
            kfQsMax = qsMax / qsScale;
        }
        kfMaxInnovUs  = maxAbsInnov;
        kfLastInnovUs = lastInnov;
    }

    LinearFuncParam updatedParam{};
    {
        std::unique_lock<std::mutex> linearFuncParamLock(linearFuncParamMutex_);
        linearFuncParam_.coefficientA = kfSlope;
        // The intercept is not stored: consumers evaluate via the point-slope form
        // anchored at (refDevTime, refSysTime), avoiding cancellation of ~1e15 numbers.
        linearFuncParam_.refDevTime = (uint64_t)kfRefDevTicks;
        linearFuncParam_.refSysTime = (uint64_t)kfRefSysUs;
        linearFuncParam_.devTime    = devTimestamp;
        linearFuncParam_.sysTime    = sysTimestamp;
        updatedParam                = linearFuncParam_;

        auto        &param      = linearFuncParam_;
        const double ticksPerMs = static_cast<double>(deviceTimeFreq_) / 1000.0;
        LOG_DEBUG("LinearParam update! N={}, A={:.3f}us/ms, ref=({}tick,{}us), cur=({}tick,{}us), rmsUs={:.1f}", queueSize, param.coefficientA * ticksPerMs,
                  param.refDevTime, param.refSysTime, param.devTime, param.sysTime, residualRmsUs);
        // Fit diagnostics: adaptive process-noise range and innovation extremes over the
        // batch. lastInnov is the newest sample's one-step residual; maxInnov flags an
        // outlier RTT sample; nisAvg/qs show how far the adaptive noise ramped up.
        LOG_DEBUG("[fit-diag] A={:.3f}us/ms nisAvg={:.2f} qs=[{:.2e},{:.2e}] lastInnov={:.0f}us maxInnov={:.0f}us rttNoiseMax={:.1f}",
                  param.coefficientA * ticksPerMs, kfNisAvg, kfQsMin, kfQsMax, kfLastInnovUs, kfMaxInnovUs, kfMaxNoiseScale);
    }
    auto mapperUpdate = timestampMapper_.update(updatedParam, residualRmsUs);
    if(mapperUpdate.resetHandled) {
        LOG_DEBUG("Timestamp mapper reset handled: generation={}, deviceShift={:.3f}ms, mode={}", mapperUpdate.generation, mapperUpdate.resetDeviceShiftMs,
                  mapperUpdate.resetApplied ? "replace" : "transition");
    }
    else if(mapperUpdate.smoothed) {
        LOG_DEBUG_INTVL_MS(30000, "Timestamp mapper update: generation={}, transition=({:.3f},{:.3f})ms, phase={:.1f}us, slopeDelta={:.6f}us/ms",
                           mapperUpdate.generation, mapperUpdate.transitionStartDeviceMs, mapperUpdate.transitionEndDeviceMs, mapperUpdate.phaseCorrectionUs,
                           mapperUpdate.slopeCorrectionUsPerMs);
    }
    else if(mapperUpdate.initialized) {
        LOG_DEBUG("Timestamp mapper initialized: generation={}", mapperUpdate.generation);
    }
    // notify
    linearFuncParamCondVar_.notify_all();
}

void GlobalTimestampFitter::ensureFitting() {
    OBTimeSample timeSample{};
    OBDeviceTime devTime{};
    bool         calc = false;

    // Hold samplingOpMutex_ for the whole bootstrap so the background fittingLoop
    // cannot interleave its own sampling/queue updates with ours.
    std::lock_guard<std::mutex> opLock(samplingOpMutex_);

    {
        uint8_t                      count = 0;
        std::unique_lock<std::mutex> lock(sampleMutex_);
        while(samplingQueue_.size() < 6 && count < 6) {
            ++count;
            // Drop the lock during the blocking I/O so other threads (e.g. reFitting) can progress.
            lock.unlock();
            bool ok = acquireSample(timeSample, devTime);
            lock.lock();
            if(!ok) {
                continue;
            }
            if(!samplingQueue_.empty() && (devTime.time < samplingQueue_.back().deviceTimestamp)) {
                samplingQueue_.clear();
                timestampMapper_.reset();
            }
            needCalculation_ = true;
            calc             = true;
            samplingQueue_.push_back({ timeSample.time, devTime.time, getSampleNoiseScale(timeSample) });
            std::this_thread::sleep_for(std::chrono::milliseconds(40));  // short interval
        }
        if(samplingQueue_.size() < 4) {
            LOG_WARN("Error, sampling queue size is too small: {}", samplingQueue_.size());
            return;
        }
    }

    if(calc) {
        calcLinearParam(timeSample.time, devTime.time);
    }
}

bool GlobalTimestampFitter::acquireSample(OBTimeSample &timeSample, OBDeviceTime &devTime) {
    try {
        auto                  owner          = getOwner();
        auto                  propertyServer = owner->getPropertyServer();
        utils::TransferTiming timing;
        devTime    = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME, PROP_ACCESS_INTERNAL, &timing);
        timeSample = rttWindow_.estimate(timing.send.startUs, timing.send.endUs);
        if(timeSample.rtt > maxValidRtt_) {
            LOG_DEBUG("Get device time rtt is too large! rtt={}", timeSample.rtt);
            return false;
        }
        LOG_DEBUG("start={}us, steady={}us, dev={}tick, rtt={}us, rttRef={}us, rttOutlier={}, rttReal={}us", timing.send.startUs, timeSample.time, devTime.time,
                  timeSample.rtt, timeSample.rttRef, timeSample.rttOutlier, timing.recv.endUs - timing.send.startUs);
        return true;
    }
    catch(...) {
        return false;
    }
}

double GlobalTimestampFitter::getSampleNoiseScale(const OBTimeSample &timeSample) const {
    if(!timeSample.rttOutlier) {
        return 1.0;
    }

    constexpr uint64_t kRef           = 1;
    const double       relativeExcess = ((std::max)(0.0, (double)timeSample.rtt - (double)timeSample.rttRef)) / (double)(std::max)(timeSample.rttRef, kRef);
    return (std::min)(KF_RTT_OUTLIER_NOISE_SCALE_MAX, 1.0 + relativeExcess * relativeExcess);
}

GlobalTimestampFitter::UpdateResult GlobalTimestampFitter::updateSampleQueue(const OBTimeSample &timeSample, const OBDeviceTime &devTime) {
    // Snapshot current fit and compare residual jump against the latest accepted sample.
    // Absolute residual drift is handled by periodic refits and the device-level mapper; only
    // abrupt discontinuities (e.g. device time retimed by PTP or timestamp jumps) rebuild the queue.
    double coefficientA      = 0;
    double refDevTime        = 0;
    double refSysTime        = 0;
    double residualUs        = 0;
    double residualJumpUs    = 0;
    bool   haveDiscontinuity = false;
    {
        std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
        if(linearFuncParam_.coefficientA > 0) {
            coefficientA = linearFuncParam_.coefficientA;
            refDevTime   = (double)linearFuncParam_.refDevTime;
            refSysTime   = (double)linearFuncParam_.refSysTime;
        }
    }
    if(coefficientA > 0) {
        std::unique_lock<std::mutex> lock(sampleMutex_);
        auto                         size = samplingQueue_.size();
        if(size >= MATURE_QUEUE_SIZE) {
            double predicted = refSysTime + coefficientA * ((double)devTime.time - refDevTime);
            residualUs       = (double)timeSample.time - predicted;

            const auto &lastSample     = samplingQueue_.back();
            double      lastPredicted  = refSysTime + coefficientA * ((double)lastSample.deviceTimestamp - refDevTime);
            double      lastResidualUs = (double)lastSample.systemTimestamp - lastPredicted;
            residualJumpUs             = residualUs - lastResidualUs;
            haveDiscontinuity          = std::abs(residualJumpUs) > (double)RESIDUAL_JUMP_THRESHOLD_US;
        }
    }

    // RttWindow has already identified this request as delayed. Do not let it update
    // the fit if its estimated host timestamp also disagrees with the last accepted
    // device-to-host delta. In particular, compare against rttRef (the normal-path
    // RTT), never against the delayed request's raw RTT, which would let an outlier
    // enlarge its own tolerance.
    const uint64_t residualLimitUs = (std::max)(RTT_OUTLIER_RESIDUAL_MIN_US, RTT_OUTLIER_RESIDUAL_RTT_K * timeSample.rttRef);
    if(timeSample.rttOutlier
       && (std::abs(residualUs) > static_cast<double>(residualLimitUs) || std::abs(residualJumpUs) > static_cast<double>(residualLimitUs))) {
        LOG_WARN("Drop global timestamp sample: RTT outlier rtt={}us ref={}us, residual={:.1f}us, residualJump={:.1f}us exceeds {}us", timeSample.rtt,
                 timeSample.rttRef, residualUs, residualJumpUs, residualLimitUs);
        return UpdateResult::Rejected;
    }

    // Discontinuity suspected: collect REVERIFY_SAMPLES extra samples and classify each as agreeing
    // (same residual jump direction from the pre-jump baseline) or normal. Decision is by vote.
    if(haveDiscontinuity) {
        bool sign = residualJumpUs > 0;
        struct R {
            OBTimeSample ts;
            OBDeviceTime dt;
            bool         agree;
        };
        std::vector<R> reverify;
        reverify.reserve(REVERIFY_SAMPLES);

        for(uint32_t i = 0; i < REVERIFY_SAMPLES; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(REVERIFY_INTERVAL_MS));
            if(sampleLoopExit_.load()) {
                return UpdateResult::Rejected;
            }
            OBTimeSample ts{};
            OBDeviceTime dt{};
            if(!acquireSample(ts, dt)) {
                continue;
            }
            double predicted = refSysTime + coefficientA * ((double)dt.time - refDevTime);
            double r         = (double)ts.time - predicted;
            double jump      = r - (residualUs - residualJumpUs);
            bool   agree     = (std::abs(jump) > (double)RESIDUAL_JUMP_THRESHOLD_US) && ((jump > 0) == sign);
            reverify.push_back({ ts, dt, agree });
        }

        size_t agreeCount = 1;  // trigger itself counts
        for(const auto &p: reverify) {
            if(p.agree) {
                ++agreeCount;
            }
        }
        size_t totalCount = 1 + reverify.size();

        if(agreeCount >= DRIFT_MIN_AGREE) {
            // Real drift: rebuild queue from agreeing samples only. Mixing old samples (fit to a
            // stale regime) with new ones biases the refit, so clear completely.
            std::unique_lock<std::mutex> lock(sampleMutex_);
            timestampMapper_.reset();
            samplingQueue_.clear();
            samplingQueue_.push_back({ timeSample.time, devTime.time, getSampleNoiseScale(timeSample) });
            for(const auto &p: reverify) {
                if(p.agree) {
                    samplingQueue_.push_back({ p.ts.time, p.dt.time, getSampleNoiseScale(p.ts) });
                }
            }
            needCalculation_ = true;
            LOG_INFO("Timestamp discontinuity confirmed (residual jump {:.1f}ms, residual {:.1f}ms, {}/{} agree), queue rebuilt with {} fresh samples",
                     residualJumpUs / 1000.0, residualUs / 1000.0, agreeCount, totalCount, samplingQueue_.size());
            return samplingQueue_.size() >= 4 ? UpdateResult::Ready : UpdateResult::NotMature;
        }

        // Trigger looks like noise. Fold non-agreeing reverify samples into the queue
        // (perfectly normal samples, no point wasting the I/O).
        std::unique_lock<std::mutex> lock(sampleMutex_);
        size_t                       kept = 0;
        for(const auto &p: reverify) {
            if(p.agree)
                continue;
            if(samplingQueue_.size() >= maxQueueSize_) {
                samplingQueue_.pop_front();
            }
            if(!samplingQueue_.empty() && (p.dt.time < samplingQueue_.back().deviceTimestamp)) {
                samplingQueue_.clear();
                timestampMapper_.reset();
            }
            samplingQueue_.push_back({ p.ts.time, p.dt.time, getSampleNoiseScale(p.ts) });
            ++kept;
        }
        if(kept > 0) {
            needCalculation_ = true;
        }
        LOG_DEBUG("Residual jump {:.1f}ms not confirmed ({}/{} agree), trigger discarded, kept {} normal reverify samples", residualJumpUs / 1000.0, agreeCount,
                  totalCount, kept);
        if(!kept) {
            return UpdateResult::Rejected;
        }
        return samplingQueue_.size() >= 4 ? UpdateResult::Ready : UpdateResult::NotMature;
    }

    // Normal path: enqueue with size cap and out-of-order protection.
    std::unique_lock<std::mutex> lock(sampleMutex_);
    if(samplingQueue_.size() >= maxQueueSize_) {
        samplingQueue_.pop_front();
    }
    if(!samplingQueue_.empty() && (devTime.time < samplingQueue_.back().deviceTimestamp)) {
        samplingQueue_.clear();
        timestampMapper_.reset();
    }
    samplingQueue_.push_back({ timeSample.time, devTime.time, getSampleNoiseScale(timeSample) });
    needCalculation_ = true;
    return samplingQueue_.size() >= 4 ? UpdateResult::Ready : UpdateResult::NotMature;
}

void GlobalTimestampFitter::fittingLoop() {
    const int MAX_RETRY_COUNT = 5;

    int retryCount = 0;
    do {
        // Consume the bootstrap signal before acquiring samplingOpMutex_.
        needBootstrap_ = false;

        OBTimeSample timeSample{};
        OBDeviceTime devTime{};
        bool         shouldRetry   = false;
        bool         longRetryWait = false;
        uint32_t     retryInterval = 0;

        // Hold samplingOpMutex_ only for sample + update + fit. Retry waits are
        // performed after releasing it so ensureFitting() can preempt between cycles.
        {
            std::lock_guard<std::mutex> opLock(samplingOpMutex_);

            UpdateResult updateResult;
            if(acquireSample(timeSample, devTime)) {
                updateResult = updateSampleQueue(timeSample, devTime);
            }
            else {
                updateResult = UpdateResult::Rejected;
            }
            if(updateResult == UpdateResult::Rejected) {
                shouldRetry = true;
                retryCount++;
                if(retryCount > MAX_RETRY_COUNT) {
                    std::unique_lock<std::mutex> lock(sampleMutex_);
                    retryInterval = refreshIntervalMsec_;
                    if(samplingQueue_.size() >= MATURE_QUEUE_SIZE) {
                        retryInterval *= 3;
                    }
                    longRetryWait = true;
                    retryCount    = 0;
                }
                else {
                    retryInterval = 100;
                }
            }
            else {
                retryCount = 0;
                if(updateResult == UpdateResult::Ready) {
                    calcLinearParam(timeSample.time, devTime.time);
                }
            }
        }

        if(shouldRetry) {
            if(longRetryWait) {
                std::unique_lock<std::mutex> lock(sampleMutex_);
                LOG_DEBUG("The global timestamp sample was rejected several times. Sleep for {}ms and retry", retryInterval);
                sampleCondVar_.wait_for(lock, std::chrono::milliseconds(retryInterval));
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(retryInterval));
            }
            continue;
        }

        // Wait. Fast path while bootstrapping (queue < 4); slower once we have enough samples.
        std::unique_lock<std::mutex> lock(sampleMutex_);
        uint32_t                     interval;
        if(samplingQueue_.size() < 4) {
            interval = 50;
        }
        else if(samplingQueue_.size() >= MATURE_QUEUE_SIZE) {
            interval = refreshIntervalMsec_ * 10;
        }
        else {
            interval = refreshIntervalMsec_;
        }
        sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval), [this]() { return sampleLoopExit_.load() || needBootstrap_.load(); });
        if(needBootstrap_.load()) {
            // Sleep briefly to ensure enough interval from reFitting for accurate sampling.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } while(!sampleLoopExit_);

    LOG_DEBUG("GlobalTimestampFitter fittingLoop exit");
}

}  // namespace libobsensor
