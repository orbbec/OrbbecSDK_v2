#include "PTPDataPort.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfileFactory.hpp"

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
#include "ptp/ObWinPTPHost.hpp"
#else
#include "ptp/ObLinuxPTPHost.hpp"
#endif

namespace libobsensor {

PTPDataPort::PTPDataPort(std::shared_ptr<const PTPSourcePortInfo> portInfo) : portInfo_(portInfo) {}

PTPDataPort::~PTPDataPort() {
    TRY_EXECUTE(if(ptpHost_) {
        ptpHost_->destroy();
        ptpHost_.reset();
    });
}

std::shared_ptr<const SourcePortInfo> PTPDataPort::getSourcePortInfo() const {
    return portInfo_;
}

bool PTPDataPort::timerSyncWithHost(TimerSyncWithHostCallback callback) {
//    if(!ptpHost_) {
//#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
//        ptpHost_ = std::make_shared<ObWinPTPHost>(portInfo_->localMac, "localAddress", portInfo_->address, portInfo_->port, portInfo_->mac);
//#else
//        ptpHost_ = std::make_shared<ObLinuxPTPHost>(portInfo_->localMac, "localAddress", portInfo_->address, portInfo_->port, portInfo_->mac);
//#endif
//    }
//
//    if(ptpHost_) {
//        ptpHost_->setPTPTimeSyncCallback([callback] {
//            if(callback) {
//                callback();
//            }
//        });
//        ptpHost_->timeSync();
//    }
    
    return true;
}

}  // namespace libobsensor