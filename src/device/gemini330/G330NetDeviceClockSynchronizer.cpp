#include "G330NetDeviceClockSynchronizer.hpp"
#include "ethernet/PTPDataPort.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

G330NetDeviceClockSynchronizer::G330NetDeviceClockSynchronizer(IDevice *owner, const std::shared_ptr<ISourcePort> &backend)
    : DeviceComponentBase(owner), backend_(backend), isClockSync_(false) {}

void G330NetDeviceClockSynchronizer::setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) {
    if(timestampResetConfig.enable) {
    }
    LOG_WARN("Timestamp reset config is not supported for G330 net device!");
}

OBDeviceTimestampResetConfig G330NetDeviceClockSynchronizer::getTimestampResetConfig() {
    OBDeviceTimestampResetConfig currentTimestampResetConfig;
    memset(&currentTimestampResetConfig, 0, sizeof(OBDeviceTimestampResetConfig));
    LOG_WARN("Get timestamp reset config is not supported for G330 net device!");
    return currentTimestampResetConfig;
}

void G330NetDeviceClockSynchronizer::timestampReset() {
    LOG_WARN("Timestamp reset is not supported for G330 net device!");
}

void G330NetDeviceClockSynchronizer::timerSyncWithHost() {
    if(isClockSync_) {
        return;
    }
    isClockSync_ = true;
    auto ptpPort = std::dynamic_pointer_cast<PTPDataPort>(backend_);
    ptpPort->timerSyncWithHost();
    isClockSync_ = false;
}

}  // namespace libobsensor
