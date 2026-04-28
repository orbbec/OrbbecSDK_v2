// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GlobalTimestampFitter.hpp"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "environment/EnvConfig.hpp"

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

GlobalTimestampFitter::GlobalTimestampFitter(IDevice *owner)
    : DeviceComponentBase(owner), enable_(false), sampleLoopExit_(false), linearFuncParam_({ 0, 0, 0, 0, 0 }), maxValidRtt_(MAX_VALID_RTT) {
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

    auto propServer = owner->getPropertyServer();
    if(propServer->isPropertySupported(OB_PROP_TIMER_RESET_SIGNAL_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->registerAccessCallback(OB_PROP_TIMER_RESET_SIGNAL_BOOL, [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                reFitting(false);
            }
        });
    }

    LOG_DEBUG("GlobalTimestampFitter created: maxQueueSize_={}, refreshIntervalMsec_={}", maxQueueSize_, refreshIntervalMsec_);
}

GlobalTimestampFitter::~GlobalTimestampFitter() {
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
        lastSysTime_ = linearFuncParam_.sysTime;
        auto &param  = linearFuncParam_;
        LOG_DEBUG("GetLinearFuncParam: A={}, ref=({},{}), cur=({},{})", param.coefficientA, param.refDevTime, param.refSysTime, param.devTime, param.sysTime);
    }

    return linearFuncParam_;
}

void GlobalTimestampFitter::reFitting(bool async) {
    if(!enable_) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(sampleMutex_);
        needCalculation_ = true;
        samplingQueue_.clear();
        sampleCondVar_.notify_one();
    }

    if(!async) {
        ensureFitting();
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
    }
    LOG_DEBUG("GlobalTimestampFitter@{} enable state changed: {}", reinterpret_cast<uint64_t>(this), enable_);
}

bool GlobalTimestampFitter::isEnabled() const {
    return enable_;
}

void GlobalTimestampFitter::calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp) {
    // Use the first set of data as offset to prevent overflow during calculation
    double offset_x      = 0;
    double offset_y      = 0;
    double Exx           = 0;
    double Exy           = 0;
    double Eyy           = 0;
    double W             = 0;
    double meanDx        = 0;
    double meanDy        = 0;
    size_t queueSize     = 0;
    double residualRmsUs = 0;

    {
        std::unique_lock<std::mutex> lock(sampleMutex_);

        if(!needCalculation_) {
            return;
        }

        // 1. Use the first element as a base offset to prevent precision loss
        // when converting large uint64_t to double.
        offset_x  = (double)(samplingQueue_.front().deviceTimestamp);
        offset_y  = (double)(samplingQueue_.front().systemTimestamp);
        queueSize = samplingQueue_.size();

        // Exponential decay weight: w_i = exp(-lambda * age_ms)
        // age_ms = newest device timestamp - sample device timestamp (device clock in ms)
        // lambda = ln(2) / (halfLife * 1000); when halfLife == 0, all weights = 1 (unweighted)
        // Use equal weights during early convergence (queueSize < ewlrThreshold) to avoid
        // staircase artifacts; switch to EWLR only after enough samples are accumulated.
        const size_t ewlrThreshold    = 15;
        double       newestDevTs      = (double)samplingQueue_.back().deviceTimestamp;
        double       effective_lambda = (queueSize >= ewlrThreshold && decayHalfLifeSec_ > 0.0) ? std::log(2.0) / (decayHalfLifeSec_ * 1000.0) : 0.0;

        // 2. First pass: compute weighted means.
        double sumWdx = 0.0, sumWdy = 0.0;
        for(const auto &item: samplingQueue_) {
            double age_ms = newestDevTs - (double)item.deviceTimestamp;
            double w      = (effective_lambda > 0.0) ? std::exp(-effective_lambda * age_ms) : 1.0;
            W += w;
            sumWdx += w * (double)(item.deviceTimestamp - offset_x);
            sumWdy += w * (double)(item.systemTimestamp - offset_y);
        }
        meanDx = sumWdx / W;
        meanDy = sumWdy / W;

        // 3. Second pass: weighted sums of squares and cross-products.
        for(const auto &item: samplingQueue_) {
            double age_ms = newestDevTs - (double)item.deviceTimestamp;
            double w      = (effective_lambda > 0.0) ? std::exp(-effective_lambda * age_ms) : 1.0;
            double dx     = ((double)(item.deviceTimestamp - offset_x)) - meanDx;
            double dy     = ((double)(item.systemTimestamp - offset_y)) - meanDy;
            Exx += w * dx * dx;
            Exy += w * dx * dy;
            Eyy += w * dy * dy;
        }

        if(std::abs(Exx) < 1e-9) {
            LOG_DEBUG("LinearParam update error! QueueSize: {}", queueSize);
            return;
        }
        needCalculation_ = false;

        // Compute weighted residual RMS (in us) to assess fit quality:
        //   SSres = Σw·(dy - A·dx)^2 = Eyy - A·Exy   (since A = Exy/Exx)
        //   rms   = sqrt(SSres / W)
        // This is device-frequency independent because dy is in microseconds.
        double slope = Exy / Exx;
        double SSres = Eyy - slope * Exy;
        if(SSres < 0.0)
            SSres = 0.0;  // guard against tiny negative due to floating-point error
        residualRmsUs = std::sqrt(SSres / W);
    }

    {
        std::unique_lock<std::mutex> linearFuncParamLock(linearFuncParamMutex_);
        double                       new_slope = Exy / Exx;
        double                       avgX      = meanDx + offset_x;
        double                       avgY      = meanDy + offset_y;

        linearFuncParam_.coefficientA = new_slope;
        // OLS intercept (avgY - new_slope * avgX) is not stored: consumers always evaluate
        // via the point-slope form anchored at (refDevTime, refSysTime), which avoids the
        // catastrophic cancellation of subtracting two ~1e15 numbers.
        linearFuncParam_.refDevTime = (uint64_t)avgX;
        linearFuncParam_.refSysTime = (uint64_t)avgY;
        linearFuncParam_.devTime    = devTimestamp;
        linearFuncParam_.sysTime    = sysTimestamp;
        // residualRmsUs: diagnostic only; drift is handled in updateSampleQueue.

        auto &param = linearFuncParam_;
        LOG_DEBUG("LinearParam update! N={}, A={}, ref=({},{}), cur=({},{}), rmsUs={:.1f}", queueSize, param.coefficientA, param.refDevTime, param.refSysTime,
                  param.devTime, param.sysTime, residualRmsUs);
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
        uint8_t count = 0;
        sampleCondVar_.notify_all();
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
            needCalculation_ = true;
            calc             = true;
            samplingQueue_.push_back({ timeSample.time, devTime.time });
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
        LOG_DEBUG("start={}, steady={}, dev={}, rtt={}, rttRef={}, rttReal={}", timing.send.startUs, timeSample.time, devTime.time, timeSample.rtt,
                  timeSample.rttRef, timing.recv.endUs - timing.send.startUs);
        return true;
    }
    catch(...) {
        return false;
    }
}

bool GlobalTimestampFitter::updateSampleQueue(const OBTimeSample &timeSample, const OBDeviceTime &devTime) {
    // Snapshot current fit and compare residual jump against the latest accepted sample.
    // Absolute residual drift is expected to be handled by EWLR + calculator-side EMA; only
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
        double predicted = refSysTime + coefficientA * ((double)devTime.time - refDevTime);
        residualUs       = (double)timeSample.time - predicted;

        std::unique_lock<std::mutex> lock(sampleMutex_);
        if(!samplingQueue_.empty()) {
            const auto &lastSample     = samplingQueue_.back();
            double      lastPredicted  = refSysTime + coefficientA * ((double)lastSample.deviceTimestamp - refDevTime);
            double      lastResidualUs = (double)lastSample.systemTimestamp - lastPredicted;
            residualJumpUs             = residualUs - lastResidualUs;
            haveDiscontinuity          = std::abs(residualJumpUs) > (double)RESIDUAL_JUMP_THRESHOLD_US;
        }
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
                return false;
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
            samplingQueue_.clear();
            samplingQueue_.push_back({ timeSample.time, devTime.time });
            for(const auto &p: reverify) {
                if(p.agree) {
                    samplingQueue_.push_back({ p.ts.time, p.dt.time });
                }
            }
            needCalculation_ = true;
            LOG_INFO("Timestamp discontinuity confirmed (residual jump {:.1f}ms, residual {:.1f}ms, {}/{} agree), queue rebuilt with {} fresh samples",
                     residualJumpUs / 1000.0, residualUs / 1000.0, agreeCount, totalCount, samplingQueue_.size());
            return samplingQueue_.size() >= 4;
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
            }
            samplingQueue_.push_back({ p.ts.time, p.dt.time });
            ++kept;
        }
        if(kept > 0) {
            needCalculation_ = true;
        }
        LOG_DEBUG("Residual jump {:.1f}ms not confirmed ({}/{} agree), trigger discarded, kept {} normal reverify samples", residualJumpUs / 1000.0, agreeCount,
                  totalCount, kept);
        return kept > 0 && samplingQueue_.size() >= 4;
    }

    // Normal path: enqueue with size cap and out-of-order protection.
    std::unique_lock<std::mutex> lock(sampleMutex_);
    if(samplingQueue_.size() >= maxQueueSize_) {
        samplingQueue_.pop_front();
    }
    if(!samplingQueue_.empty() && (devTime.time < samplingQueue_.back().deviceTimestamp)) {
        samplingQueue_.clear();
    }
    samplingQueue_.push_back({ timeSample.time, devTime.time });
    needCalculation_ = true;
    return samplingQueue_.size() >= 4;
}

void GlobalTimestampFitter::fittingLoop() {
    const int MAX_RETRY_COUNT = 5;

    int retryCount = 0;
    do {
        OBTimeSample timeSample{};
        OBDeviceTime devTime{};

        // Hold samplingOpMutex_ for one work cycle (sample + update + fit). Released
        // before wait_for so ensureFitting() can preempt between cycles.
        {
            std::lock_guard<std::mutex> opLock(samplingOpMutex_);

            if(!acquireSample(timeSample, devTime)) {
                retryCount++;
                if(retryCount > MAX_RETRY_COUNT) {
                    std::unique_lock<std::mutex> lock(sampleMutex_);
                    auto                         interval = refreshIntervalMsec_;
                    if(samplingQueue_.size() >= 15) {
                        interval *= 10;
                    }
                    LOG_DEBUG("The device time RTT has reached the upper limit several times. Sleep for {}ms and retry", interval);
                    sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval));
                    retryCount = 0;
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                continue;
            }
            retryCount = 0;

            // Unified queue update: drift detection + reverify + enqueue + clear.
            if(updateSampleQueue(timeSample, devTime)) {
                calcLinearParam(timeSample.time, devTime.time);
            }
        }

        // Wait. Fast path while bootstrapping (queue < 4); slower once we have enough samples.
        std::unique_lock<std::mutex> lock(sampleMutex_);
        uint32_t                     interval;
        if(samplingQueue_.size() < 4) {
            interval = 50;
        }
        else if(samplingQueue_.size() >= 15) {
            interval = refreshIntervalMsec_ * 10;
        }
        else {
            interval = refreshIntervalMsec_;
        }
        sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval), [this]() { return sampleLoopExit_.load(); });
    } while(!sampleLoopExit_);

    LOG_DEBUG("GlobalTimestampFitter fittingLoop exit");
}

}  // namespace libobsensor
