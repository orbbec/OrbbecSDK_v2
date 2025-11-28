// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARDeviceMonitor.hpp"
#include "protocol/LiDARProtocol.hpp"

namespace libobsensor {

LiDARDeviceMonitor::LiDARDeviceMonitor(IDevice *owner, std::shared_ptr<ISourcePort> dataPort)
    : DeviceComponentBase(owner) {
    vendorDataPort_ = std::dynamic_pointer_cast<IVendorDataPort>(dataPort);
    if(!vendorDataPort_) {
        throw std::runtime_error("LiDARDeviceMonitor: data port must be a vendor data port!");
    }

    auto activityRecorder = owner->getComponentT<IDeviceActivityRecorder>(OB_DEV_COMPONENT_DEVICE_ACTIVITY_RECORDER, false);
    if(activityRecorder) {
        activityRecorder_ = activityRecorder.get();
    }
}

LiDARDeviceMonitor::~LiDARDeviceMonitor() noexcept {
}

OBDeviceState LiDARDeviceMonitor::getCurrentDeviceState() const {
    LOG_ERROR("LiDAR device doesn't support heartbeat and fetching state right now!");
    return 0x00;
}

int LiDARDeviceMonitor::registerStateChangedCallback(DeviceStateChangedCallback callback) {
    utils::unusedVar(callback);
    throw libobsensor::not_implemented_exception("LiDAR device doesn't support heartbeat and fetching state right now!");
    return -1;
}

void LiDARDeviceMonitor::unregisterStateChangedCallback(int callbackId) {
    utils::unusedVar(callbackId);
}

void LiDARDeviceMonitor::enableHeartbeat() {
    throw libobsensor::not_implemented_exception("LiDAR device doesn't support heartbeat and fetching state right now!");
}

void LiDARDeviceMonitor::disableHeartbeat() {
    throw libobsensor::not_implemented_exception("LiDAR device doesn't support heartbeat and fetching state right now!");
}

bool LiDARDeviceMonitor::isHeartbeatEnabled() const {
    return false;
}

void LiDARDeviceMonitor::pauseHeartbeat() {
    throw libobsensor::not_implemented_exception("LiDAR device doesn't support heartbeat and fetching state right now!");
}

void LiDARDeviceMonitor::resumeHeartbeat() {
    throw libobsensor::not_implemented_exception("LiDAR device doesn't support heartbeat and fetching state right now!");
}

void LiDARDeviceMonitor::sendAndReceiveData(const uint8_t *sendData, uint32_t sendDataSize, uint8_t *receiveData, uint32_t *receiveDataSize) {
    auto recvLen     = vendorDataPort_->sendAndReceive(sendData, sendDataSize, receiveData, *receiveDataSize);
    *receiveDataSize = recvLen;

    // update active time
    if(activityRecorder_) {
        activityRecorder_->touch(DeviceActivity::Command);
    }
}

}  // namespace libobsensor
