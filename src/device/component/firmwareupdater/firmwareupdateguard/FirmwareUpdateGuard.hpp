#pragma once

#include "IDevice.hpp"
#include "component/DeviceComponentBase.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"

#include <memory>

namespace libobsensor {

class IFirmwareUpdateGuard : public DeviceComponentBase {
public:
    IFirmwareUpdateGuard(IDevice *owner);
    virtual ~IFirmwareUpdateGuard() = default;

    virtual void prepare() = 0;
    virtual void revert()  = 0;
};

class FirmwareUpdateGuardFactory : public DeviceComponentBase {
public:
    FirmwareUpdateGuardFactory(IDevice *owner);
    virtual ~FirmwareUpdateGuardFactory() noexcept override = default;

    std::shared_ptr<IFirmwareUpdateGuard> create();
};

class G2XLNetFirmwareUpdateGuard : public IFirmwareUpdateGuard {
public:
    G2XLNetFirmwareUpdateGuard(IDevice *owner);
    virtual ~G2XLNetFirmwareUpdateGuard() noexcept override;

    virtual void prepare() override;
    virtual void revert() override;

private:
    bool isGlobalTimestampEnabled_;
    std::shared_ptr<GlobalTimestampFitter> globalTimestampFilter_;
};
} // namespace libobsnsor