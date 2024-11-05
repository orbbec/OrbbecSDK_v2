#pragma once

#include "IDeviceClockSynchronizer.hpp"
#include "DeviceComponentBase.hpp"
#include "ethernet/PTPDataPort.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"


namespace libobsensor {
class G330NetDeviceClockSynchronizer : public IDeviceClockSynchronizer, public DeviceComponentBase {
public:
    G330NetDeviceClockSynchronizer(IDevice *owner, const std::shared_ptr<ISourcePort> &backend);
    virtual ~G330NetDeviceClockSynchronizer() = default;

    void                         setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) override;
    OBDeviceTimestampResetConfig getTimestampResetConfig() override;
    void                         timestampReset() override;
    void                         timerSyncWithHost() override;

private:
    std::shared_ptr<ISourcePort> backend_;
    std::shared_ptr<PTPDataPort> ptpPort_;
    std::atomic<bool>            isClockSync_;

    std::shared_ptr<GlobalTimestampFitter> globalTimestampFitter_;

};

}  // namespace libobsensor
