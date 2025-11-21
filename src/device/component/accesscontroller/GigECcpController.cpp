// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if defined(BUILD_NET_PAL)

#include "GigECcpController.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"

namespace libobsensor {

GigECcpController::GigECcpController(const std::shared_ptr<const IDeviceEnumInfo> &info) : enumInfo_(info) {
    // create gvcp transmitor
    auto portInfo    = enumInfo_->getSourcePortInfoList().front();
    auto netPortInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
    gvcpTransmit_    = std::make_shared<GVCPTransmit>(netPortInfo);
    // checkCcpCapability
    checkCcpCapability();
}

GigECcpController::~GigECcpController() {
    keepaliveStopped_ = true;
    keepaliveCv_.notify_all();
    if(keepaliveThread_.joinable()) {
        keepaliveThread_.join();
    }
    TRY_EXECUTE({ releaseControl(); });
}

void GigECcpController::checkCcpCapability() {
    // Read GVCP capability register
    auto res = gvcpTransmit_->readRegister(GVCP_CAPABILITY_REGISTER);
    if(res.first != GEV_STATUS_SUCCESS) {
        ccpSupported_ = false;
        return;
    }
    ccpSupported_ = true;
}

void GigECcpController::acquireControl(OBDeviceAccessMode accessMode) {
    if(!ccpSupported_) {
        throw libobsensor::unsupported_operation_exception("The current device doesn't support GigE CCP");
    }

    uint16_t ccpValue = 0;
    // check access mode
    switch(accessMode) {
    case OB_DEVICE_MONITOR_ACCESS:
        break;
    case OB_DEVICE_EXCLUSIVE_ACCESS:
        // exclusive access: write CCP register to acquire the privilege
        ccpValue = GVCP_CCP_EXCLUSIVE_ACCESS | GVCP_CCP_CONTROL_ACCESS;
        break;
    case OB_DEVICE_DEFAULT_ACCESS:
    case OB_DEVICE_CONTROL_ACCESS:
        // control access: writer CCP register to acquire the privilege
        accessMode = OB_DEVICE_CONTROL_ACCESS;
        ccpValue   = GVCP_CCP_CONTROL_ACCESS;
        break;
    default:
        std::ostringstream oss;
        oss << "Unsupported or invalid access mode: " << accessMode;
        throw libobsensor::unsupported_operation_exception(oss.str());
        break;
    }

    if(accessMode == OB_DEVICE_MONITOR_ACCESS) {
        // read CCP register to detect the privilege
        auto res = gvcpTransmit_->readRegister(GVCP_CCP_REGISTER);
        if(res.first != GEV_STATUS_SUCCESS || ((res.second & GVCP_CCP_EXCLUSIVE_ACCESS) != 0)) {
            std::ostringstream oss;
            oss << "The requested 'monitor access' privilege conflicts with the current control channel privilege: " << res.second;
            throw libobsensor::access_denied_exception(oss.str());
        }
        accessMode_ = accessMode;
        return;
    }

    // exclusive and control access: write CCP register to acquire the privilege
    auto status = gvcpTransmit_->writeRegister(GVCP_CCP_REGISTER, ccpValue);
    if(status != GEV_STATUS_SUCCESS) {
        std::ostringstream oss;
        if(status == GEV_STATUS_ACCESS_DENIED) {
            oss << "The requested '" << accessMode << "' privilege conflicts with the current control channel privilege";
            throw libobsensor::access_denied_exception(oss.str());
        }
        else {
            oss << "Failed to acquire the '" << accessMode << "' privilege. Status code: " << status;
            throw libobsensor::io_exception(oss.str());
        }
    }
    accessMode_ = accessMode;
    // start thread to keep alive
    keepaliveStopped_ = false;
    keepaliveThread_  = std::thread([this, ccpValue]() {
        // read heartbeat timeout register
        uint32_t timeout = 0;
        auto     res     = gvcpTransmit_->readRegister(GVCP_HEARTBEAT_TIMEOUT_REGISTER);
        if(res.first != GEV_STATUS_SUCCESS) {
            timeout = 1000;  // use default value
            LOG_WARN("Failt to read hearbeat timeout register, use the default timeout({}) instant. Status code: {:#04x}", timeout, res.first);
        }
        else {
            // By GigE verison 2.2
            // It is recommended for the application to send a heartbeat message three times within that device heartbeat timeout period.
            // This ensures at least two heartbeat UDP packets can be lost without the connection being automatically closed by the device
            // because of heartbeat timeout.Depending on the quality of the network connection, it is possible to adjust these numbers
            timeout = res.second / 3;
            if(timeout < 500) {
                // By GigE version 2.2
                timeout = 500;
            }
        }

        std::mutex                   mutex;
        std::unique_lock<std::mutex> lock(mutex);
        while(!keepaliveStopped_) {
            // Read CCP register to keepalive
            res = gvcpTransmit_->readRegister(GVCP_CCP_REGISTER);
            if(res.first != GEV_STATUS_SUCCESS) {
                LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP Keepalive", 3000, spdlog::level::warn,
                           "Failed to read CCP register, ignore it. Status code: {:#04x}. Ignore", res.first);
            }
            res.second = (res.second & GVCP_CCP_MASK);
            if(res.second != ccpValue) {
                LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP Keepalive", 3000, spdlog::level::warn, "Expected CCP register value {:#04x} but got {:#04x}. Ignore",
                           ccpValue, res.second);
            }
            keepaliveCv_.wait_for(lock, std::chrono::milliseconds(timeout), [&]() { return keepaliveStopped_.load(); });
        }
        LOG_DEBUG("GVCP Keepalive exists now");
        keepaliveStopped_ = true;
    });
}

void GigECcpController::releaseControl() {
    if(!ccpSupported_) {
        return;
    }
    // restore the CCP to open access
    if((accessMode_ != OB_DEVICE_MONITOR_ACCESS) && (accessMode_ != OB_DEVICE_ACCESS_DENIED)) {
        auto status = gvcpTransmit_->writeRegister(GVCP_CCP_REGISTER, GVCP_CCP_OPEN_ACCESS);
        if(status != GEV_STATUS_SUCCESS) {
            LOG_DEBUG("Failed to restore the CCP to open access, ignore it. Status code: {:#04x}", status);
        }
        else {
            LOG_DEBUG("Restore the CCP to open access successfully");
        }
    }
}

}  // namespace libobsensor

#endif  // BUILD_NET_PAL
