#pragma once
#include <string>
#include <vector>
#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

class IDeviceSyncConfigurator {
public:
    virtual ~IDeviceSyncConfigurator() = default;

    virtual OBMultiDeviceSyncConfig getSyncConfig()                                                = 0;
    virtual void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) = 0;
    virtual uint16_t                getSupportedSyncModeBitmap()                                   = 0;
    virtual void                    triggerCapture()                                               = 0;
};

}  // namespace libobsensor
