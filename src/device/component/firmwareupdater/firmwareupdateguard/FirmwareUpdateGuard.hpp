#pragma once

#include "IDevice.hpp"
#include "component/DeviceComponentBase.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"

#include <memory>

namespace libobsensor {

class IFirmwareUpdateGuard {
public:
    virtual ~IFirmwareUpdateGuard() = default;

    virtual void prepare() = 0;
    virtual void revert()  = 0;
};

class G2XLNetFirmwareUpdateGuard : public IFirmwareUpdateGuard, public DeviceComponentBase {
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