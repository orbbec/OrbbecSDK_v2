// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GlobalTimestampFitter.hpp"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {
GlobalTimestampFitter::GlobalTimestampFitter(IDevice *owner)
    : DeviceComponentBase(owner), enable_(false), sampleLoopExit_(false), linearFuncParam_({ 0, 0, 0, 0 }) {
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

    auto                  propServer = owner->getPropertyServer();
    std::vector<uint32_t> supportedProps;
    if(propServer->isPropertySupported(OB_PROP_TIMER_RESET_SIGNAL_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        supportedProps.push_back(OB_PROP_TIMER_RESET_SIGNAL_BOOL);
    }
    if(propServer->isPropertySupported(OB_STRUCT_DEVICE_TIME, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        supportedProps.push_back(OB_STRUCT_DEVICE_TIME);
    }
    if(!supportedProps.empty()) {
        propServer->registerAccessCallback(supportedProps, [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                reFitting();
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
    return linearFuncParam_;
}

void GlobalTimestampFitter::reFitting() {
    std::unique_lock<std::mutex> lock(sampleMutex_);
    samplingQueue_.clear();
    sampleCondVar_.notify_one();
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

void GlobalTimestampFitter::enable(bool en) {
    if(en == enable_) {
        return;
    }
    enable_ = en;
    if(enable_) {
        sampleThread_ = std::thread(&GlobalTimestampFitter::fittingLoop, this);
        std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
        linearFuncParamCondVar_.wait_for(lock, std::chrono::milliseconds(1000));
    }
    else {
        sampleLoopExit_ = true;
        sampleCondVar_.notify_one();
        if(sampleThread_.joinable()) {
            sampleThread_.join();
        }
        std::unique_lock<std::mutex> lock(sampleMutex_);
        samplingQueue_.clear();
    }
    LOG_DEBUG("GlobalTimestampFitter@{} enable state changed: {}", reinterpret_cast<uint64_t>(this), enable_);
}

bool GlobalTimestampFitter::isEnabled() const {
    return enable_;
}

void GlobalTimestampFitter::fittingLoop() {
    const int      MAX_RETRY_COUNT = 5;
    const uint64_t MAX_VALID_RTT   = 20000;  // 10ms

    int retryCount = 0;
    do {

        uint64_t     sysTspUsec = 0;
        OBDeviceTime devTime;

        try {
            auto owner          = getOwner();
            auto propertyServer = owner->getPropertyServer();

            auto sysTsp1Usec = utils::getNowTimesUs();
            devTime          = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME);
            auto sysTsp2Usec = utils::getNowTimesUs();
            sysTspUsec       = (sysTsp2Usec + sysTsp1Usec) / 2;
            devTime.rtt      = sysTsp2Usec - sysTsp1Usec;
            if(devTime.rtt > MAX_VALID_RTT) {
                LOG_DEBUG("Get device time rtt is too large! rtt={}", devTime.rtt);
                throw std::runtime_error("RTT too large");
            }
            LOG_TRACE("sys={}, dev={}, rtt={}", sysTspUsec, devTime.time, devTime.rtt);
        }
        catch(...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            retryCount++;
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

            samplingQueue_.push_back({ sysTspUsec, devTime.time });

            if(samplingQueue_.size() < 4) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
        }

        // Use the first set of data as offset to prevent overflow during calculation
        uint64_t offset_x = samplingQueue_.front().deviceTimestamp;
        uint64_t offset_y = samplingQueue_.front().systemTimestamp;
        double   Ex       = 0;
        double   Exx      = 0;
        double   Ey       = 0;
        double   Exy      = 0;
        auto     it       = samplingQueue_.begin();
        while(it != samplingQueue_.end()) {
            auto systemTimestamp = it->systemTimestamp - offset_y;
            auto deviceTimestamp = it->deviceTimestamp - offset_x;
            Ex += deviceTimestamp;
            Exx += deviceTimestamp * deviceTimestamp;
            Ey += systemTimestamp;
            Exy += deviceTimestamp * systemTimestamp;
            it++;
        }

        {
            std::unique_lock<std::mutex> linearFuncParamLock(linearFuncParamMutex_);
            // Linear regression to find a and b: y=ax+b
            linearFuncParam_.coefficientA = (Exy * samplingQueue_.size() - Ex * Ey) / (samplingQueue_.size() * Exx - Ex * Ex);
            linearFuncParam_.constantB  = (Exx * Ey - Exy * Ex) / (samplingQueue_.size() * Exx - Ex * Ex) + offset_y - linearFuncParam_.coefficientA * offset_x;
            linearFuncParam_.checkDataX = devTime.time;
            linearFuncParam_.checkDataY = sysTspUsec;

            // auto fixDevTsp = (double)devTime *linearFuncParam_.coefficientA + linearFuncParam_.constantB;
            // auto fixDiff   = fixDevTsp -sysTspUsec;
            // LOG_TRACE("a = {}, b = {}, fix={}, diff={}", linearFuncParam_.coefficientA, linearFuncParam_.constantB, fixDevTsp, fixDiff);

            LOG_DEBUG_INTVL("GlobalTimestampFitter update: coefficientA = {}, constantB = {}", linearFuncParam_.coefficientA, linearFuncParam_.constantB);
            linearFuncParamCondVar_.notify_all();
        }

        {
            std::unique_lock<std::mutex> lock(sampleMutex_);
            auto                         interval = refreshIntervalMsec_;
            if(samplingQueue_.size() >= 15) {
                interval *= 10;
            }
            sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval));
        }

    } while(!sampleLoopExit_ && retryCount <= MAX_RETRY_COUNT);

    if(retryCount > MAX_RETRY_COUNT) {
        LOG_ERROR("GlobalTimestampFitter fittingLoop retry count exceed max retry count!");
    }

    LOG_DEBUG("GlobalTimestampFitter fittingLoop exit");
}

}  // namespace libobsensor
