#include "FirmwareUpdateGuard.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {
IFirmwareUpdateGuard::IFirmwareUpdateGuard(IDevice* owner) : DeviceComponentBase(owner) {}
    
FirmwareUpdateGuardFactory::FirmwareUpdateGuardFactory(IDevice* owner) : DeviceComponentBase(owner) {}

std::shared_ptr<IFirmwareUpdateGuard> FirmwareUpdateGuardFactory::create() {
    auto pid            = getOwner()->getInfo()->pid_;
    auto connectionType = getOwner()->getInfo()->connectionType_;
    switch (pid)
    {
    case 0x0671 /* G2XL */:
        if (connectionType == "Ethernet") {
            return std::make_shared<G2XLNetFirmwareUpdateGuard>(getOwner());
        }
        else {
            return nullptr;
        }
    default:
        LOG_WARN("Unsupported update guard for device: pid {}, connectionType {}", pid, connectionType);
        return nullptr;
    }
}

G2XLNetFirmwareUpdateGuard::G2XLNetFirmwareUpdateGuard(IDevice* owner) : IFirmwareUpdateGuard(owner) {
    globalTimestampFilter_ = getOwner()->getComponentT<GlobalTimestampFitter>(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER).get();
    isGlobalTimestampEnabled_ = globalTimestampFilter_->isEnabled();
    prepare();
}

G2XLNetFirmwareUpdateGuard::~G2XLNetFirmwareUpdateGuard() noexcept {
    revert();
}

void G2XLNetFirmwareUpdateGuard::prepare() {
    globalTimestampFilter_->enable(false);
}

void G2XLNetFirmwareUpdateGuard::revert() {
    globalTimestampFilter_->enable(isGlobalTimestampEnabled_);
}

} // namespace libonsensor