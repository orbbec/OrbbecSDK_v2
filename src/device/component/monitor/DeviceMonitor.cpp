// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceMonitor.hpp"
#include "protocol/Protocol.hpp"
#include "property/InternalProperty.hpp"
#include "logger/LoggerSnWrapper.hpp"  // Must be included last to override log macros

namespace libobsensor {

const uint16_t MAX_RECV_DATA_SIZE = 1024;

const std::string &DeviceMonitor::GetCurrentSN() const {
    auto owner = getOwner();
    if(owner) {
        return owner->getSn();
    }

    static std::string unknown = "Unknown";
    return unknown;
}

DeviceMonitor::DeviceMonitor(IDevice *owner, std::shared_ptr<ISourcePort> dataPort)
    : DeviceComponentBase(owner),
      cbIdCounter_(0),
      heartbeatAndFetchStateThreadStarted_(false),
      heartbeatEnabled_(false),
      heartbeatPaused_(false),
      firmwareLogEnabled_(false),
      hbRecvData_(MAX_RECV_DATA_SIZE),
      hbSendData_(MAX_RECV_DATA_SIZE) {
    vendorDataPort_ = std::dynamic_pointer_cast<IVendorDataPort>(dataPort);
    if(!vendorDataPort_) {
        THROW_INVALID_PARAM_EXCEPTION("DeviceMonitor: data port must be a vendor data port!");
    }

    auto activityRecorder = owner->getComponentT<IDeviceActivityRecorder>(OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER, false);
    if(activityRecorder) {
        activityRecorder_ = activityRecorder.get();
    }
}

DeviceMonitor::~DeviceMonitor() noexcept {
    if(heartbeatEnabled_ && !heartbeatPaused_) {
        TRY_EXECUTE(disableHeartbeat());
    }

    if(firmwareLogEnabled_) {
        TRY_EXECUTE(disableFirmwareLog());
    }

    if(heartbeatAndFetchStateThreadStarted_) {
        TRY_EXECUTE(stop());
    }
}

void DeviceMonitor::start() {
    if(heartbeatAndFetchStateThreadStarted_) {
        LOG_DEBUG("Heartbeat and fetch state thread already started!");
        return;
    }
    heartbeatAndFetchStateThreadStarted_ = true;
    heartbeatAndFetchStateThread_        = std::thread([this]() {
        const uint32_t HEARTBEAT_INTERVAL_MS = 3000;
        while(heartbeatAndFetchStateThreadStarted_) {
            std::unique_lock<std::mutex> lock(commMutex_);
            heartbeatAndFetchStateThreadCv_.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS),
                                                            [this]() { return !heartbeatAndFetchStateThreadStarted_; });
            if(!heartbeatAndFetchStateThreadStarted_) {
                break;
            }
            heartbeatAndFetchState();
        }
    });
}

void DeviceMonitor::stop() {
    if(!heartbeatAndFetchStateThreadStarted_) {
        LOG_DEBUG("Heartbeat and fetch state thread not started!");
        return;
    }
    heartbeatAndFetchStateThreadStarted_ = false;
    heartbeatAndFetchStateThreadCv_.notify_one();
    heartbeatAndFetchStateThread_.join();
    LOG_DEBUG("Heartbeat and fetch state thread stopped!");
}

void DeviceMonitor::heartbeatAndFetchState() {
    bool emitNextHeartBeatImmediately = false;
    do {
        protocol::initHeartbeatAndStateReq(hbSendData_.data());
        uint16_t respDataSize = 1024;  // excepted size
        auto     res = protocol::execute(vendorDataPort_, hbSendData_.data(), sizeof(protocol::HeartbeatAndStateReq), hbRecvData_.data(), &respDataSize);
        if(!protocol::checkStatus(0, res, false)) {
            utils::sleepMs(50);
            if(!heartbeatAndFetchStateThreadStarted_) {
                break;
            }
            continue;
        }

        protocol::HeartbeatAndStateResp *resp;
        BEGIN_TRY_EXECUTE({ resp = protocol::parseHeartbeatAndStateResp(hbRecvData_.data(), respDataSize); })
        CATCH_EXCEPTION_AND_EXECUTE({ continue; })

        // Heartbeat state value
        // ==================================================
        // |--[0, 31]bit --|--[32, 62]bit --|-- [63]bit ---|
        // |---------------|----------------|-------------|
        // |--ERROR msg  --|--WARNING msg --| CACHE flag  |
        // ==================================================

        // The highest bit is the device status information cache flag bit. If the device also caches other status information, the next heartbeat should be
        // executed immediately.
        emitNextHeartBeatImmediately = (bool)(resp->state >> 63);

        // Get status code (excluding cache flag 1 bit)
        devState_ = (OBDeviceState)(resp->state & 0x7FFFFFFFFFFFFFFF);

        // The remaining information is msg
        auto msgSize = resp->header.sizeInHalfWords * 2 - 10;  // Remove header.error and state, the remaining is msg

        // Callback when firmware returns non-0 status code
        if(0 != devState_ || msgSize) {
            auto msg = std::string(resp->message, msgSize);
            LOG_INFO("Firmware State/Log ({}):\n{}", devState_, msg);
            std::lock_guard<std::mutex> lock(stateChangedCallbacksMutex_);
            for(auto &callback: stateChangedCallbacks_) {
                callback.second(devState_, msg);
            }
        }

        // update active time
        if(activityRecorder_) {
            activityRecorder_->touch(DeviceActivity::Command);
        }
    } while(emitNextHeartBeatImmediately);
}

OBDeviceState DeviceMonitor::getCurrentDeviceState() const {
    if(!heartbeatAndFetchStateThreadStarted_) {
        LOG_WARN("Heartbeat and fetch state thread not started, the state may expired!");
    }
    return devState_;
}

int DeviceMonitor::registerStateChangedCallback(DeviceStateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(stateChangedCallbacksMutex_);
    auto                        callbackId = cbIdCounter_++;
    stateChangedCallbacks_[callbackId]     = callback;
    return callbackId;
}

void DeviceMonitor::unregisterStateChangedCallback(int callbackId) {
    std::lock_guard<std::mutex> lock(stateChangedCallbacksMutex_);
    stateChangedCallbacks_.erase(callbackId);
}

void DeviceMonitor::enableHeartbeat() {
    if(heartbeatEnabled_) {
        LOG_DEBUG("Heartbeat already enabled!");
        return;
    }

    auto            owner = getOwner();
    OBPropertyValue value;
    value.intValue    = 1;
    auto propAccessor = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    propAccessor->setPropertyValue(OB_PROP_HEARTBEAT_BOOL, value);
    heartbeatEnabled_ = true;
    heartbeatPaused_  = false;
    if(!firmwareLogEnabled_ && !heartbeatAndFetchStateThreadStarted_) {
        auto propServer = owner->getComponentT<IPropertyServer>(OB_DEV_COMPONENT_PROPERTY_SERVER);
        if(propServer->isPropertySupported(OB_PROP_DEVICE_LOG_SEVERITY_LEVEL_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT<int32_t>(OB_PROP_DEVICE_LOG_SEVERITY_LEVEL_INT, OB_LOG_SEVERITY_DEBUG);
        }

        start();
    }
    LOG_DEBUG("Heartbeat enabled!");
}

void DeviceMonitor::disableHeartbeat() {
    if(!heartbeatEnabled_) {
        LOG_DEBUG("Heartbeat already disabled!");
        return;
    }
    auto owner = getOwner();

    OBPropertyValue value;
    value.intValue    = 0;
    auto propAccessor = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    propAccessor->setPropertyValue(OB_PROP_HEARTBEAT_BOOL, value);

    heartbeatEnabled_ = false;
    heartbeatPaused_  = false;
    if(!firmwareLogEnabled_ && heartbeatAndFetchStateThreadStarted_) {
        stop();
    }
    LOG_DEBUG("Heartbeat disabled!");
}

bool DeviceMonitor::isHeartbeatEnabled() const {
    return heartbeatEnabled_;
}

void DeviceMonitor::pauseHeartbeat() {
    if(heartbeatPaused_ || !heartbeatEnabled_) {
        LOG_DEBUG("Heartbeat already paused!");
        return;
    }
    auto            owner = getOwner();
    OBPropertyValue value;
    value.intValue    = 0;
    auto propAccessor = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
    propAccessor->setPropertyValue(OB_PROP_HEARTBEAT_BOOL, value);
    if(!firmwareLogEnabled_ && heartbeatAndFetchStateThreadStarted_) {
        stop();
    }
    heartbeatPaused_ = true;
    LOG_DEBUG("Heartbeat paused!");
}

void DeviceMonitor::resumeHeartbeat() {
    if(!heartbeatPaused_) {
        LOG_DEBUG("Heartbeat already resumed!");
        return;
    }

    if(heartbeatEnabled_) {
        // resume heartbeat enable state if it was enabled before
        auto            owner = getOwner();
        OBPropertyValue value;
        value.intValue    = 1;
        auto propAccessor = owner->getComponentT<IBasicPropertyAccessor>(OB_DEV_COMPONENT_MAIN_PROPERTY_ACCESSOR);
        propAccessor->setPropertyValue(OB_PROP_HEARTBEAT_BOOL, value);
    }

    if(!firmwareLogEnabled_ && !heartbeatAndFetchStateThreadStarted_) {
        start();
    }
    heartbeatPaused_ = false;
    LOG_DEBUG("Heartbeat resumed!");
}

void DeviceMonitor::sendAndReceiveData(const uint8_t *sendData, uint32_t sendDataSize, uint8_t *receiveData, uint32_t *receiveDataSize) {
    std::lock_guard<std::mutex> lock(commMutex_);
    auto                        recvLen = vendorDataPort_->sendAndReceive(sendData, sendDataSize, receiveData, *receiveDataSize);
    *receiveDataSize                    = recvLen;

    // update active time
    if(activityRecorder_) {
        activityRecorder_->touch(DeviceActivity::Command);
    }
}

void DeviceMonitor::enableFirmwareLog() {
    if(firmwareLogEnabled_) {
        LOG_DEBUG("Firmware log already enabled!");
        return;
    }

    if(!heartbeatEnabled_ && !heartbeatAndFetchStateThreadStarted_) {
        auto owner      = getOwner();
        auto propServer = owner->getComponentT<IPropertyServer>(OB_DEV_COMPONENT_PROPERTY_SERVER);
        if(propServer->isPropertySupported(OB_PROP_DEVICE_LOG_SEVERITY_LEVEL_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT<int32_t>(OB_PROP_DEVICE_LOG_SEVERITY_LEVEL_INT, OB_LOG_SEVERITY_DEBUG);
        }

        start();
    }

    firmwareLogEnabled_ = true;
    LOG_DEBUG("Firmware log enabled!");
}
void DeviceMonitor::disableFirmwareLog() {
    if(!firmwareLogEnabled_) {
        LOG_DEBUG("Firmware log already disabled!");
        return;
    }
    firmwareLogEnabled_ = false;
    if(!heartbeatEnabled_ && heartbeatAndFetchStateThreadStarted_) {
        stop();
    }
    LOG_DEBUG("Firmware log disabled!");
}

bool DeviceMonitor::isFirmwareLogEnabled() const {
    return firmwareLogEnabled_;
}

}  // namespace libobsensor
