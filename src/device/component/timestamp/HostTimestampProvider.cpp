// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HostTimestampProvider.hpp"
#include "frame/Frame.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {

HostTimestampProvider ::HostTimestampProvider() {
    loadConfig();
}

void HostTimestampProvider::loadConfig() {
    auto        envConfig = EnvConfig::getInstance();
    std::string value;
    auto        type = OB_CLOCK_TYPE_REALTIME;  // default

    if(envConfig->getStringValue("Device.ClockSource", value)) {
        if(value == "Realtime") {
            type = OB_CLOCK_TYPE_REALTIME;
        }
        else if(value == "Monotonic") {
            type = OB_CLOCK_TYPE_MONOTONIC;
        }
        else {
            LOG_DEBUG("Unknown clock source, use realtime instead");
        }
    }
    setClockType(type);
}

void HostTimestampProvider::setClockType(OBClockType type) {
    clockType_.store(type, std::memory_order_relaxed);
    LOG_INFO("Update clock type to: {}", type);
}

OBClockType HostTimestampProvider::getClockType() const {
    return clockType_.load(std::memory_order_relaxed);
}

uint64_t HostTimestampProvider::getTimeUs() const {
    return (getClockType() == OB_CLOCK_TYPE_MONOTONIC) ? utils::getSteadyTimeUs() : utils::getNowTimesUs();
}

uint64_t HostTimestampProvider::getTimeMs() const {
    return (getClockType() == OB_CLOCK_TYPE_MONOTONIC) ? utils::getSteadyTimeMs() : utils::getNowTimesMs();
}

void HostTimestampProvider::applyHostTimestamp(const std::shared_ptr<Frame> &frame) const {
    if(!frame) {
        return;
    }

    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        // Select steady time as frame system time
        auto steadyTime = frame->getSteadyTimeStampUsec();
        frame->setSystemTimeStampUsec(steadyTime);
        if(frame->isDeviceTimestampFromHost()) {
            frame->setTimeStampUsec(steadyTime);
        }
    }
}

}  // namespace libobsensor
