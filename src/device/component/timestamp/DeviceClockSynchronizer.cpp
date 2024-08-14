#include "DeviceClockSynchronizer.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

DeviceClockSynchronizer::DeviceClockSynchronizer(IDevice *owner, uint64_t deviceClockFreqIn, uint64_t deviceClockFreqOut)
    : DeviceComponentBase(owner), deviceClockFreqIn_(deviceClockFreqIn), deviceClockFreqOut_(deviceClockFreqOut), isTimestampResetConfigInit_(false) {}

void DeviceClockSynchronizer::setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) {
    if(isTimestampResetConfigInit_ && 0 == memcmp(&currentTimestampResetConfig_, &timestampResetConfig, sizeof(OBDeviceTimestampResetConfig))) {
        LOG_DEBUG("New timestamp reset config is same as current device timestamp reset config, the upgrade process would not execute!");
        return;
    }

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_ENABLE_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_TIMER_RESET_ENABLE_BOOL, timestampResetConfig.enable);
    }
    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, timestampResetConfig.timestamp_reset_signal_output_enable);
    }
    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_DELAY_US_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT(OB_PROP_TIMER_RESET_DELAY_US_INT, timestampResetConfig.timestamp_reset_delay_us);
    }

    currentTimestampResetConfig_ = timestampResetConfig;
    isTimestampResetConfigInit_  = true;
}

OBDeviceTimestampResetConfig DeviceClockSynchronizer::getTimestampResetConfig() {
    if(isTimestampResetConfigInit_) {
        return currentTimestampResetConfig_;
    }

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();

    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_ENABLE_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        currentTimestampResetConfig_.enable = propertyServer->getPropertyValueT<bool>(OB_PROP_TIMER_RESET_ENABLE_BOOL);
    }
    else {
        currentTimestampResetConfig_.enable = true;
    }

    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        currentTimestampResetConfig_.timestamp_reset_signal_output_enable =
            propertyServer->getPropertyValueT<bool>(OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL);
    }
    else {
        currentTimestampResetConfig_.timestamp_reset_signal_output_enable = true;
    }

    if(propertyServer->isPropertySupported(OB_PROP_TIMER_RESET_DELAY_US_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        currentTimestampResetConfig_.timestamp_reset_delay_us = propertyServer->getPropertyValueT<int>(OB_PROP_TIMER_RESET_DELAY_US_INT);
    }

    isTimestampResetConfigInit_ = true;
    return currentTimestampResetConfig_;
}

void DeviceClockSynchronizer::timestampReset() {
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setPropertyValueT(OB_PROP_TIMER_RESET_ENABLE_BOOL, true);
}

void DeviceClockSynchronizer::timerSyncWithHost() {
    const uint32_t MINI_HOST_DEVICE_TIME_DIFF = 5000;  // us
    const uint32_t MAX_REPEAT_TIME            = 10;
    uint8_t        repeated                   = 0;
    uint64_t       rtt                        = 0;

    while(repeated < MAX_REPEAT_TIME) {
        {
            auto owner          = getOwner();
            auto propertyServer = owner->getPropertyServer();

            auto now = utils::getNowTimesUs();
            OBDeviceTime devTsp;
            devTsp.time = static_cast<uint64_t>(static_cast<double>(now) / 1000000 * deviceClockFreqOut_);
            devTsp.rtt  = static_cast<uint64_t>(static_cast<double>(rtt) / 1000000 * deviceClockFreqOut_);
            propertyServer->setStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME, devTsp);
            uint64_t after = utils::getNowTimesUs();
            rtt            = after - now;
        }
        if(repeated == 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            repeated++;
            continue;
        }
        {
            auto     owner          = getOwner();
            auto     propertyServer = owner->getPropertyServer();
            uint64_t now            = utils::getNowTimesUs();
            auto     devTsp         = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME);
            uint64_t after          = utils::getNowTimesUs();
            uint64_t nowDev         = static_cast<uint64_t>(static_cast<double>(devTsp.time) / deviceClockFreqIn_ * 1000000);
            double   diff           = std::fabs(static_cast<double>(now) - (after + now) / 2);
            if(diff > 0xFFFFFFFF) {
                int64_t adjustedDiff = (now & 0xFFFFFFFF) - (nowDev & 0xFFFFFFFF) - (rtt & 0xFFFFFFFF) / 2;
                diff                 = static_cast<double>(adjustedDiff > 0 ? adjustedDiff : -adjustedDiff);
            }
            if(diff <= static_cast<double>(MINI_HOST_DEVICE_TIME_DIFF)) {
                break;
            }
            LOG_DEBUG("Device/Host time diff: {} us, rtt: {} us, repeated: {}", diff, rtt, repeated);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        repeated++;
    }

    if(repeated >= MAX_REPEAT_TIME) {
        throw io_exception(utils::string::to_string() << "syncDeviceTime failed after retry " << repeated << " times, rtt=" << rtt);
    }
}

}  // namespace libobsensor
