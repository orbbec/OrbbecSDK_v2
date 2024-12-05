#include "FirmwareUpdateGuard.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {
G2XLNetFirmwareUpdateGuard::G2XLNetFirmwareUpdateGuard(IDevice* owner) : DeviceComponentBase(owner) {
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