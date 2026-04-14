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
    : DeviceComponentBase(owner), enable_(false), sampleLoopExit_(false), linearFuncParam_({ 0, 0, 0, 0 }), maxValidRtt_(MAX_VALID_RTT) {
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
}

LinearFuncParam GlobalTimestampFitter::getLinearFuncParam() {
    std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
    if(lastCheckDataY != linearFuncParam_.checkDataY) {
        lastCheckDataY = linearFuncParam_.checkDataY;
        auto &param    = linearFuncParam_;
        LOG_DEBUG("GetLinearFuncParam: coefficientA: {}, refSysTime: {}, checkDataX: {}, checkDataY: {}", param.coefficientA, param.refSysTime, param.checkDataX,
                  param.checkDataY);
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
        }
        {
            std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
            linearFuncParam_ = { 0, 0, 0, 0 };
        }
    }
    LOG_DEBUG("GlobalTimestampFitter@{} enable state changed: {}", reinterpret_cast<uint64_t>(this), enable_);
}

bool GlobalTimestampFitter::isEnabled() const {
    return enable_;
}

void GlobalTimestampFitter::calcLinearParam(uint64_t sysTimestamp, uint64_t devTimestamp) {
    // Use the first set of data as offset to prevent overflow during calculation
    double offset_x  = 0;
    double offset_y  = 0;
    double Exx       = 0;
    double Exy       = 0;
    double meanDx    = 0;
    double meanDy    = 0;
    size_t queueSize = 0;

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
        double W = 0.0, sumWdx = 0.0, sumWdy = 0.0;
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
        }

        if(std::abs(Exx) < 1e-9) {
            LOG_DEBUG("LinearParam update error! QueueSize: {}", queueSize);
            return;
        }
        needCalculation_ = false;
    }

    {
        std::unique_lock<std::mutex> linearFuncParamLock(linearFuncParamMutex_);
        double                       new_slope = Exy / Exx;
        double                       avgX      = meanDx + offset_x;
        double                       avgY      = meanDy + offset_y;
        double                       new_B     = avgY + new_slope * ((double)devTimestamp - avgX);

        linearFuncParam_.coefficientA = new_slope;
        // new_B is the regression-predicted system timestamp (us) at devTimestamp
        linearFuncParam_.refSysTime   = new_B;
        linearFuncParam_.checkDataX   = devTimestamp;
        linearFuncParam_.checkDataY   = sysTimestamp;

        auto &param = linearFuncParam_;
        LOG_DEBUG("LinearParam update! QueueSize: {}, coefficientA: {}, refSysTime: {}, checkDataX: {}, checkDataY: {}", queueSize, param.coefficientA,
                  param.refSysTime, param.checkDataX, param.checkDataY);
    }
    // notify
    linearFuncParamCondVar_.notify_all();
}

void GlobalTimestampFitter::ensureFitting() {
    uint64_t     steadyTspUsec = 0;
    OBDeviceTime devTime{};
    bool         calc = false;

    {
        auto    owner          = getOwner();
        auto    propertyServer = owner->getPropertyServer();
        uint8_t count          = 0;

        sampleCondVar_.notify_all();
        std::unique_lock<std::mutex> lock(sampleMutex_);
        while(samplingQueue_.size() < 6 && count < 6) {
            ++count;
            utils::TransferTiming timing;
            devTime       = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME, PROP_ACCESS_INTERNAL, &timing);
            steadyTspUsec = (timing.send.startUs + timing.send.endUs) / 2;
            devTime.rtt   = timing.send.endUs - timing.send.startUs;
            if(devTime.rtt > maxValidRtt_) {
                LOG_DEBUG("Get device time rtt is too large! rtt={}", devTime.rtt);
                continue;
            }
            LOG_DEBUG("steady={}, dev={}, rtt={}", steadyTspUsec, devTime.time, devTime.rtt);

            needCalculation_ = true;
            calc             = true;
            samplingQueue_.push_back({ steadyTspUsec, devTime.time });
            std::this_thread::sleep_for(std::chrono::milliseconds(40));  // short interval
        }
        if(samplingQueue_.size() < 4) {
            LOG_WARN("Error, sampling queue size is too small: {}", samplingQueue_.size());
            return;
        }
    }

    if(calc) {
        calcLinearParam(steadyTspUsec, devTime.time);
    }
}

void GlobalTimestampFitter::fittingLoop() {
    const int MAX_RETRY_COUNT = 5;

    int retryCount = 0;
    do {
        uint64_t     steadyTspUsec = 0;
        OBDeviceTime devTime{};

        try {
            auto owner          = getOwner();
            auto propertyServer = owner->getPropertyServer();

            utils::TransferTiming timing;
            devTime       = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME, PROP_ACCESS_INTERNAL, &timing);
            steadyTspUsec = (timing.send.startUs + timing.send.endUs) / 2;
            devTime.rtt   = timing.send.endUs - timing.send.startUs;
            if(devTime.rtt > maxValidRtt_) {
                THROW_INVALID_DATA_EXCEPTION(utils::string::to_string() << "Get device time rtt is too large! rtt=" << devTime.rtt);
            }
            LOG_DEBUG("steady={}, dev={}, rtt={}", steadyTspUsec, devTime.time, devTime.rtt);
        }
        catch(...) {
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

        // Successfully obtain timestamp, the number of retries is reset to zero
        retryCount = 0;
        {
            std::unique_lock<std::mutex> lock(sampleMutex_);
            if(samplingQueue_.size() > maxQueueSize_) {
                samplingQueue_.pop_front();
            }

            // Clearing and refitting when the timestamp is out of order
            if(!samplingQueue_.empty() && (devTime.time < samplingQueue_.back().deviceTimestamp)) {
                samplingQueue_.clear();
            }

            needCalculation_ = true;
            samplingQueue_.push_back({ steadyTspUsec, devTime.time });
            if(samplingQueue_.size() < 4) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
        }

        // calc linear param
        calcLinearParam(steadyTspUsec, devTime.time);

        // wait for a moment
        {
            std::unique_lock<std::mutex> lock(sampleMutex_);
            auto                         interval = refreshIntervalMsec_;
            if(samplingQueue_.size() >= 15) {
                interval *= 10;
            }
            sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval), [this]() { return sampleLoopExit_.load(); });
        }
    } while(!sampleLoopExit_);

    LOG_DEBUG("GlobalTimestampFitter fittingLoop exit");
}

}  // namespace libobsensor
