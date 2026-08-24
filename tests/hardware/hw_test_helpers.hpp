// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace hw_test {

using test_utils::getFirstVideoProfile;
using test_utils::hasSensorType;
using test_utils::tryGetSensor;
using test_utils::waitForFramesetWithRetry;

inline bool supportsDepthWorkMode(const std::shared_ptr<ob::Device> &device) {
    if(!device)
        return false;
    try {
        auto mode = device->getCurrentDepthWorkMode();
        (void)mode;
        return true;
    }
    catch(...) {
        return false;
    }
}

inline bool supportsPreset(const std::shared_ptr<ob::Device> &device) {
    if(!device)
        return false;
    try {
        auto list = device->getAvailablePresetList();
        return list && list->getCount() > 0;
    }
    catch(...) {
        return false;
    }
}

inline bool supportsMultiDeviceSync(const std::shared_ptr<ob::Device> &device) {
    if(!device)
        return false;
    try {
        return device->getSupportedMultiDeviceSyncModeBitmap() != 0u;
    }
    catch(...) {
        return false;
    }
}

inline bool rebootAndReconnect(const std::shared_ptr<ob::Context> &ctx, const std::string &serial, std::shared_ptr<ob::Device> &device,
                               std::shared_ptr<ob::DeviceInfo> &devInfo) {
    const std::string sn = serial.empty() && devInfo ? (devInfo->getSerialNumber() ? devInfo->getSerialNumber() : "") : serial;
    if(device) {
        try {
            device->reboot();
        }
        catch(...) {
        }
    }
    device.reset();
    devInfo.reset();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    constexpr int kMaxWaitMs = 90000;
    constexpr int kPollMs    = 1000;
    for(int elapsed = 0; elapsed < kMaxWaitMs; elapsed += kPollMs) {
        try {
            auto devList = ctx->queryDeviceList();
            if(devList && devList->deviceCount() > 0) {
                std::shared_ptr<ob::Device> dev = sn.empty() ? devList->getDevice(0) : devList->getDeviceBySN(sn.c_str());
                if(dev && dev->getDeviceInfo()) {
                    device  = dev;
                    devInfo = device->getDeviceInfo();
                    return true;
                }
            }
        }
        catch(...) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    }
    return false;
}

class ScopeGuard {
public:
    template <typename Fn> explicit ScopeGuard(Fn &&fn) : fn_(std::forward<Fn>(fn)) {}

    ~ScopeGuard() noexcept {
        try {
            if(fn_)
                fn_();
        }
        catch(...) {
        }
    }

    ScopeGuard(const ScopeGuard &)            = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    void release() noexcept {
        fn_ = nullptr;
    }

private:
    std::function<void()> fn_;
};

class BoolPropertyGuard {
public:
    BoolPropertyGuard(std::shared_ptr<ob::Device> device, OBPropertyID propertyId) : device_(std::move(device)), propertyId_(propertyId) {
        try {
            if(device_ && device_->isPropertySupported(propertyId_, OB_PERMISSION_READ_WRITE)) {
                original_ = device_->getBoolProperty(propertyId_);
                armed_    = true;
            }
        }
        catch(...) {
        }
    }

    ~BoolPropertyGuard() noexcept {
        if(armed_) {
            try {
                device_->setBoolProperty(propertyId_, original_);
            }
            catch(...) {
            }
        }
    }

    BoolPropertyGuard(const BoolPropertyGuard &)            = delete;
    BoolPropertyGuard &operator=(const BoolPropertyGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    OBPropertyID                propertyId_;
    bool                        original_ = false;
    bool                        armed_    = false;
};

class IntPropertyGuard {
public:
    IntPropertyGuard(std::shared_ptr<ob::Device> device, OBPropertyID propertyId) : device_(std::move(device)), propertyId_(propertyId) {
        try {
            if(device_ && device_->isPropertySupported(propertyId_, OB_PERMISSION_READ_WRITE)) {
                original_ = device_->getIntProperty(propertyId_);
                armed_    = true;
            }
        }
        catch(...) {
        }
    }

    ~IntPropertyGuard() noexcept {
        if(armed_) {
            try {
                device_->setIntProperty(propertyId_, original_);
            }
            catch(...) {
            }
        }
    }

    IntPropertyGuard(const IntPropertyGuard &)            = delete;
    IntPropertyGuard &operator=(const IntPropertyGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    OBPropertyID                propertyId_;
    int                         original_ = 0;
    bool                        armed_    = false;
};

class SyncConfigGuard {
public:
    explicit SyncConfigGuard(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {
        try {
            if(device_) {
                original_ = device_->getMultiDeviceSyncConfig();
                armed_    = true;
            }
        }
        catch(...) {
        }
    }

    ~SyncConfigGuard() noexcept {
        if(armed_) {
            try {
                device_->setMultiDeviceSyncConfig(original_);
            }
            catch(...) {
            }
        }
    }

    SyncConfigGuard(const SyncConfigGuard &)            = delete;
    SyncConfigGuard &operator=(const SyncConfigGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    OBMultiDeviceSyncConfig     original_{};
    bool                        armed_ = false;
};

class DepthModeGuard {
public:
    explicit DepthModeGuard(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {
        try {
            if(device_) {
                auto mode = device_->getCurrentDepthWorkMode();
                if(mode.name)
                    originalName_ = mode.name;
                armed_ = true;
            }
        }
        catch(...) {
        }
    }

    ~DepthModeGuard() noexcept {
        if(armed_ && !originalName_.empty()) {
            try {
                device_->switchDepthWorkMode(originalName_.c_str());
            }
            catch(...) {
            }
        }
    }

    DepthModeGuard(const DepthModeGuard &)            = delete;
    DepthModeGuard &operator=(const DepthModeGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    std::string                 originalName_;
    bool                        armed_ = false;
};

class PresetGuard {
public:
    explicit PresetGuard(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {
        try {
            if(device_) {
                auto name = device_->getCurrentPresetName();
                if(name)
                    originalName_ = name;
                armed_ = true;
            }
        }
        catch(...) {
        }
    }

    ~PresetGuard() noexcept {
        if(armed_ && !originalName_.empty()) {
            try {
                device_->loadPreset(originalName_.c_str());
            }
            catch(...) {
            }
        }
    }

    PresetGuard(const PresetGuard &)            = delete;
    PresetGuard &operator=(const PresetGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    std::string                 originalName_;
    bool                        armed_ = false;
};

class ClockSyncGuard {
public:
    explicit ClockSyncGuard(std::shared_ptr<ob::Context> ctx, uint32_t restoreIntervalMs = 0) : ctx_(std::move(ctx)), restoreIntervalMs_(restoreIntervalMs) {}

    ~ClockSyncGuard() noexcept {
        try {
            if(ctx_)
                ctx_->enableDeviceClockSync(restoreIntervalMs_);
        }
        catch(...) {
        }
    }

    ClockSyncGuard(const ClockSyncGuard &)            = delete;
    ClockSyncGuard &operator=(const ClockSyncGuard &) = delete;

private:
    std::shared_ptr<ob::Context> ctx_;
    uint32_t                     restoreIntervalMs_;
};

class HeartbeatDisableGuard {
public:
    explicit HeartbeatDisableGuard(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {}

    ~HeartbeatDisableGuard() noexcept {
        try {
            if(device_)
                device_->enableHeartbeat(false);
        }
        catch(...) {
        }
    }

    HeartbeatDisableGuard(const HeartbeatDisableGuard &)            = delete;
    HeartbeatDisableGuard &operator=(const HeartbeatDisableGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
};

class FrameInterleaveGuard {
public:
    explicit FrameInterleaveGuard(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {
        if(device_ && device_->isFrameInterleaveSupported()) {
            try {
                auto name = device_->getCurrentFrameInterleaveName();
                if(name)
                    original_ = name;
            }
            catch(...) {
            }
            armed_ = true;
        }
    }

    ~FrameInterleaveGuard() noexcept {
        if(armed_ && !original_.empty()) {
            try {
                device_->loadFrameInterleave(original_.c_str());
            }
            catch(...) {
            }
        }
    }

    FrameInterleaveGuard(const FrameInterleaveGuard &)            = delete;
    FrameInterleaveGuard &operator=(const FrameInterleaveGuard &) = delete;

private:
    std::shared_ptr<ob::Device> device_;
    std::string                 original_;
    bool                        armed_ = false;
};

class ScopedSensorStream {
public:
    explicit ScopedSensorStream(std::shared_ptr<ob::Sensor> sensor) : sensor_(std::move(sensor)) {}

    ~ScopedSensorStream() noexcept {
        if(sensor_) {
            try {
                sensor_->stop();
            }
            catch(...) {
            }
        }
    }

    ScopedSensorStream(const ScopedSensorStream &)            = delete;
    ScopedSensorStream &operator=(const ScopedSensorStream &) = delete;

    void release() noexcept {
        sensor_.reset();
    }

private:
    std::shared_ptr<ob::Sensor> sensor_;
};

}  // namespace hw_test
