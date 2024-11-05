#include "G330NetDeviceClockSynchronizer.hpp"
#include "ethernet/PTPDataPort.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

G330NetDeviceClockSynchronizer::G330NetDeviceClockSynchronizer(IDevice *owner, const std::shared_ptr<ISourcePort> &backend)
    : DeviceComponentBase(owner), backend_(backend), isClockSync_(false) {
    ptpPort_               = std::dynamic_pointer_cast<PTPDataPort>(backend_);
    globalTimestampFitter_ = owner->getComponentT<GlobalTimestampFitter>(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER).get();
}

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
    BEGIN_TRY_EXECUTE({
        isClockSync_ = true;
        ptpPort_->timerSyncWithHost();
        isClockSync_ = false;
        globalTimestampFitter_->reFitting();
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get profile list params failed!");
        isClockSync_ = false;
    })
}

}  // namespace libobsensor
