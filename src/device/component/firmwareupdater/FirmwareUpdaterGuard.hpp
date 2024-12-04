#pragma once

#include "IDevice.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"

#include <memory>


namespace libobsensor {
class IFirmwareUpdaterGuard {
public:
    virtual ~IFirmwareUpdaterGuard() = default;

    virtual void enter() = 0;
    virtual void exit()  = 0;
};

class G2XLFirmwareUpdaterGuard : public IFirmwareUpdaterGuard {
public:
    G2XLFirmwareUpdaterGuard(std::shared_ptr<ob_device> device);
    ~G2XLFirmwareUpdaterGuard();

    virtual void enter() override;
    virtual void exit() override;

private:
    std::shared_ptr<ob_device> device_;

    bool is_global_timestamp_enabled_;
};
} // namespace libobsnsor