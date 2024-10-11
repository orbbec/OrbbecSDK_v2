#include "PTPDataPort.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

PTPDataPort::PTPDataPort(std::shared_ptr<const PTPSourcePortInfo> portInfo) : portInfo_(portInfo) {}

PTPDataPort::~PTPDataPort() {
    TRY_EXECUTE(if(ptpHost_) { ptpHost_->destroy(); });
}

std::shared_ptr<const SourcePortInfo> PTPDataPort::getSourcePortInfo() const {
    return portInfo_;
}

bool PTPDataPort::timerSyncWithHost() {
    if(ptpHost_) {
        ptpHost_.reset();
    }

    ptpHost_ = std::make_shared<ObPTPHost>(portInfo_->localMac, "localAddress", portInfo_->address, portInfo_->port, portInfo_->mac);
    ptpHost_->timeSync();
    return true;
}

}  // namespace libobsensor