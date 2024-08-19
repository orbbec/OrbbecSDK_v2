#include "NetPortGroup.hpp"

namespace libobsensor {
bool GroupNetSourcePortByMac(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1) {
    auto netPort0 = std::dynamic_pointer_cast<const NetSourcePortInfo>(port0);
    auto netPort1 = std::dynamic_pointer_cast<const NetSourcePortInfo>(port1);
    return netPort0->mac == netPort1->mac;
}
}  // namespace libobsensor