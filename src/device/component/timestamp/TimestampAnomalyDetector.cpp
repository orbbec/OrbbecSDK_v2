#include "TimestampAnomalyDetector.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
TimestampAnomalyDetector::TimestampAnomalyDetector(IDevice *device) : cacheTimestamp_(0), maxValidTimestampDiff_(0), cacheFps_(0) {
    auto propertyServer = device->getPropertyServer();
    deviceSyncConfigurator_ = device->getComponentT<IDeviceSyncConfigurator>(OB_DEV_COMPONENT_DEVICE_SYNC_CONFIGURATOR).get();
}

void TimestampAnomalyDetector::setCurrentFps(uint32_t fps){
    if(fps == 0) {
        maxValidTimestampDiff_ = 0;
    }
    else {
        maxValidTimestampDiff_ = static_cast<uint32_t>((1000000 / fps) * 10); // 10 frames tolerance
    }
    cacheFps_ = fps;
}

void TimestampAnomalyDetector::calculate(std::shared_ptr<Frame> frame) {
    auto syncConfig = deviceSyncConfigurator_->getSyncConfig();
    if(syncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING || syncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING) {
        return;
    }

    auto timestamp = frame->getTimeStampUsec();
    if(timestamp == 0) {
        return;
    }
    if(cacheTimestamp_ == 0) {
        cacheTimestamp_ = timestamp;
        return;
    }

    if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE)) {
        auto actualFps = frame->getMetadataValue(OB_FRAME_METADATA_TYPE_ACTUAL_FRAME_RATE);
        if(actualFps > 0 && actualFps != cacheFps_) {
            setCurrentFps(static_cast<uint32_t>(actualFps));
        }
    }

    uint64_t diff = (timestamp > cacheTimestamp_) ? (timestamp - cacheTimestamp_) : (cacheTimestamp_ - timestamp);
    if(diff > maxValidTimestampDiff_) {
        auto originalCacheTimestamp = cacheTimestamp_;
        cacheTimestamp_ = timestamp;
        throw libobsensor::invalid_value_exception("Timestamp anomaly detected, timestamp: " + std::to_string(timestamp)
                                                   + ", cacheTimestamp: " + std::to_string(originalCacheTimestamp) + " ,currentDiff: " + std::to_string(diff)
                                                   + ", maxValidTimestampDiff: " + std::to_string(maxValidTimestampDiff_));
    }
    cacheTimestamp_ = timestamp;
}

void TimestampAnomalyDetector::clear() {
    cacheTimestamp_ = 0;
    maxValidTimestampDiff_ = 0;
}
} // namespace libobsensor