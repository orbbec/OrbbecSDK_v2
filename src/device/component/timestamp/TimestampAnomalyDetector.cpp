#include "TimestampAnomalyDetector.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
TimestampAnomalyDetector::TimestampAnomalyDetector(IDevice *device)
    : lastTimestamp_(0), lastValidTimestamp_(0), maxValidTimestampDiff_(kMinTimestampDiffLimit) {
    auto config = device->getComponentT<IDeviceSyncConfigurator>(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR, false);
    if(config) {
        deviceSyncConfigurator_ = config.get();
        // Warm up device cache by preloading sync config
        (void)deviceSyncConfigurator_->getSyncConfig();
    }
}

void TimestampAnomalyDetector::setCurrentFps(uint32_t fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(fps == 0) {
        maxValidTimestampDiff_ = kMinTimestampDiffLimit;
    }
    else {
        maxValidTimestampDiff_ = static_cast<uint32_t>((1000000 / fps) * 10);  // 10 frames tolerance
        if(maxValidTimestampDiff_ < kMinTimestampDiffLimit) {
            maxValidTimestampDiff_ = kMinTimestampDiffLimit;
        }
    }
}

void TimestampAnomalyDetector::calculate(std::shared_ptr<Frame> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    TRY_EXECUTE({
        if(deviceSyncConfigurator_) {
            auto syncConfig = deviceSyncConfigurator_->getSyncConfig();
            if(syncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING || syncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING) {
                return;
            }
        }
    });

    auto timestamp = frame->getTimeStampUsec();
    if(timestamp == 0) {
        return;
    }
    if(lastTimestamp_ == 0) {
        lastTimestamp_ = lastValidTimestamp_ = timestamp;
        return;
    }

    auto     absDiff       = [](uint64_t a, uint64_t b) { return (a > b) ? (a - b) : (b - a); };
    uint64_t diff          = absDiff(timestamp, lastTimestamp_);
    uint64_t diffLastValid = absDiff(timestamp, lastValidTimestamp_);
    if(diff > maxValidTimestampDiff_ && diffLastValid > maxValidTimestampDiff_) {
        auto lastTimestamp = lastTimestamp_;
        lastTimestamp_     = timestamp;
        THROW_INVALID_DATA_EXCEPTION(utils::string::to_string()
                                     << "Timestamp anomaly detected, timestamp: " << timestamp << ", lastTimestamp: " << lastTimestamp << ", currentDiff: "
                                     << diff << ", diffLastValid: " << diffLastValid << ", maxValidTimestampDiff: " << maxValidTimestampDiff_);
    }
    lastTimestamp_      = timestamp;
    lastValidTimestamp_ = timestamp;
}

void TimestampAnomalyDetector::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastTimestamp_         = 0;
    lastValidTimestamp_    = 0;
    maxValidTimestampDiff_ = kMinTimestampDiffLimit;
}
}  // namespace libobsensor
