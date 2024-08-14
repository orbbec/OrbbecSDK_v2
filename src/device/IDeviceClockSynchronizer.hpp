#pragma once
#include <string>
#include <vector>
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

class IDeviceClockSynchronizer {
public:
    virtual ~IDeviceClockSynchronizer()                                                                                    = default;
    virtual void                         setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) = 0;
    virtual OBDeviceTimestampResetConfig getTimestampResetConfig()                                                         = 0;
    virtual void                         timestampReset()                                                                  = 0;
    virtual void                         timerSyncWithHost()                                                               = 0;
};

}  // namespace libobsensor
