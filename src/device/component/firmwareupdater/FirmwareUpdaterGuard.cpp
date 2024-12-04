#include "FirmwareUpdaterGuard.hpp"

namespace libobsensor {

G2XLFirmwareUpdaterGuard::G2XLFirmwareUpdaterGuard(std::shared_ptr<ob_device> device) : device_(device) {
    is_global_timestamp_enabled_ = device_->device->getComponentT<libobsensor::GlobalTimestampFitter>(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER)->isEnabled();
    enter();
}

G2XLFirmwareUpdaterGuard::~G2XLFirmwareUpdaterGuard() {
    exit();
}

void G2XLFirmwareUpdaterGuard::enter() {
    device_->device->getComponentT<libobsensor::GlobalTimestampFitter>(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER)->enable(false);
}

void G2XLFirmwareUpdaterGuard::exit() {
    device_->device->getComponentT<libobsensor::GlobalTimestampFitter>(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER)->enable(is_global_timestamp_enabled_);
}

} // namespace libonsensor