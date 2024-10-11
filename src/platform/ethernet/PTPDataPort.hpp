#pragma once
#include "ISourcePort.hpp"
#include "ethernet/VendorNetDataPort.hpp"
#include "ptp/ObPTPHost.hpp"

#include <string>
#include <memory>
#include <thread>
#include <iostream>

namespace libobsensor {

struct PTPSourcePortInfo : public NetSourcePortInfo {
    PTPSourcePortInfo(std::string localMac, std::string address, uint16_t port, uint16_t vendorPort, std::string mac = "unknown",
                      std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_PTP, localMac, address, port, mac, serialNumber, pid), vendorPort(vendorPort) {}

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const PTPSourcePortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort);
    };

    uint16_t vendorPort;
};

class PTPDataPort : public IPTPDataPort {

public:
    PTPDataPort(std::shared_ptr<const PTPSourcePortInfo> portInfo);
    virtual ~PTPDataPort() noexcept;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

    virtual bool timerSyncWithHost() override;

private:
    std::shared_ptr<const PTPSourcePortInfo> portInfo_;
    std::shared_ptr<ObPTPHost>               ptpHost_;
};
}