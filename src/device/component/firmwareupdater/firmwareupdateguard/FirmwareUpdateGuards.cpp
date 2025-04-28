// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FirmwareUpdateGuards.hpp"
#include "DevicePids.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
// FirmwareUpdateGuardFactory
FirmwareUpdateGuardFactory::FirmwareUpdateGuardFactory(IDevice *owner) : DeviceComponentBase(owner) {}

std::shared_ptr<IFirmwareUpdateGuard> FirmwareUpdateGuardFactory::create() {
    auto pid   = getOwner()->getInfo()->pid_;
    auto guard = std::make_shared<CompositeGuard>();

    if(std::find(G330DevPids.begin(), G330DevPids.end(), pid) != G330DevPids.end()) {
        guard->addGuard(std::make_shared<GlobalTimestampGuard>(getOwner()));
        guard->addGuard(std::make_shared<HeardbeatGuard>(getOwner()));
        return guard;
    }
    else if(std::find(DaBaiADevPids.begin(), DaBaiADevPids.end(), pid) != DaBaiADevPids.end()) {
        guard->addGuard(std::make_shared<GlobalTimestampGuard>(getOwner()));
        guard->addGuard(std::make_shared<HeardbeatGuard>(getOwner()));
        return guard;
    }
    else if(std::find(Gemini2DevPids.begin(), Gemini2DevPids.end(), pid) != Gemini2DevPids.end()) {
        guard->addGuard(std::make_shared<GlobalTimestampGuard>(getOwner()));
        guard->addGuard(std::make_shared<HeardbeatGuard>(getOwner()));
        return guard;
    }
    else if(std::find(Astra2DevPids.begin(), Astra2DevPids.end(), pid) != Astra2DevPids.end()) {
        guard->addGuard(std::make_shared<GlobalTimestampGuard>(getOwner()));
        guard->addGuard(std::make_shared<HeardbeatGuard>(getOwner()));
        return guard;
    }

    // FemtoBolt and FemtoMega upgrade differently than other devices, no need to add any guards for them
    LOG_DEBUG("Create update guard: Unsupported device pid: {}", pid);
    return guard;
}

// CompositeGuard
CompositeGuard::CompositeGuard(const std::vector<std::shared_ptr<IFirmwareUpdateGuard>> &guards) : guards_(guards) {
    preUpdate();
}

CompositeGuard::CompositeGuard(CompositeGuard &&other) noexcept {
    guards_ = std::move(other.guards_);
}

CompositeGuard &CompositeGuard::operator=(CompositeGuard &&other) noexcept {
    if(this != &other) {
        guards_ = std::move(other.guards_);
    }
    return *this;
}

CompositeGuard::~CompositeGuard() noexcept {
    postUpdate();
}

void CompositeGuard::addGuard(std::shared_ptr<IFirmwareUpdateGuard> guard) {
    if(guard) {
        guards_.emplace_back(guard);
        guard->preUpdate();
    }
}

void CompositeGuard::preUpdate() {
    for(auto &guard: guards_) {
        guard->preUpdate();
    }
}

void CompositeGuard::postUpdate() {
    for(auto it = guards_.rbegin(); it != guards_.rend(); ++it) {
        (*it)->postUpdate();
    }
}

// SeparateGuard
// NullGuard
NullGuard::NullGuard(IDevice *owner) {
    utils::unusedVar(owner);
}

void NullGuard::preUpdate() {}

void NullGuard::postUpdate() {}

// GlobalTimestampGuard
GlobalTimestampGuard::GlobalTimestampGuard(IDevice *owner) : owner_(owner), isGlobalTimestampEnabled_(false) {
    globalTimestampFilter_ = owner_->getComponentT<GlobalTimestampFitter>(OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER, false).get();
}

GlobalTimestampGuard::GlobalTimestampGuard(GlobalTimestampGuard &&other) noexcept {
    owner_                       = other.owner_;
    isGlobalTimestampEnabled_    = other.isGlobalTimestampEnabled_;
    globalTimestampFilter_       = other.globalTimestampFilter_;
    other.globalTimestampFilter_ = nullptr;
}

GlobalTimestampGuard &GlobalTimestampGuard::operator=(GlobalTimestampGuard &&other) noexcept {
    if(this != &other) {
        owner_                       = other.owner_;
        isGlobalTimestampEnabled_    = other.isGlobalTimestampEnabled_;
        globalTimestampFilter_       = other.globalTimestampFilter_;
        other.globalTimestampFilter_ = nullptr;
    }
    return *this;
}

void GlobalTimestampGuard::preUpdate() {
    if(globalTimestampFilter_) {
        isGlobalTimestampEnabled_ = globalTimestampFilter_->isEnabled();
        LOG_DEBUG("GlobalTimestampGuard: try to disable global timestamp filter, current state: {}", isGlobalTimestampEnabled_);
        TRY_EXECUTE({ globalTimestampFilter_->enable(false); });
    }
}

void GlobalTimestampGuard::postUpdate() {
    if(globalTimestampFilter_) {
        LOG_DEBUG("GlobalTimestampGuard: try to restore global timestamp filter, previous state: {}", isGlobalTimestampEnabled_);
        TRY_EXECUTE({ globalTimestampFilter_->enable(isGlobalTimestampEnabled_); });
    }
}

// HeardbeatGuard
HeardbeatGuard::HeardbeatGuard(IDevice *owner) : owner_(owner), isHeartrateEnabled_(false) {
    deviceMonitor_ = owner_->getComponentT<DeviceMonitor>(OB_DEV_COMPONENT_DEVICE_MONITOR, false).get();
}

HeardbeatGuard::HeardbeatGuard(HeardbeatGuard &&other) noexcept {
    owner_               = other.owner_;
    isHeartrateEnabled_  = other.isHeartrateEnabled_;
    deviceMonitor_       = other.deviceMonitor_;
    other.deviceMonitor_ = nullptr;
}

HeardbeatGuard &HeardbeatGuard::operator=(HeardbeatGuard &&other) noexcept {
    if(this != &other) {
        owner_               = other.owner_;
        isHeartrateEnabled_  = other.isHeartrateEnabled_;
        deviceMonitor_       = other.deviceMonitor_;
        other.deviceMonitor_ = nullptr;
    }
    return *this;
}

void HeardbeatGuard::preUpdate() {
    if(deviceMonitor_) {
        isHeartrateEnabled_ = deviceMonitor_->isHeartbeatEnabled();
        LOG_DEBUG("HeardbeatGuard: try to disable heartbeat, current state: {}", isHeartrateEnabled_);

        TRY_EXECUTE({ deviceMonitor_->disableHeartbeat(); });
    }
}

void HeardbeatGuard::postUpdate() {
    if(deviceMonitor_ && isHeartrateEnabled_) {
        LOG_DEBUG("HeardbeatGuard: try to restore heartbear, previous state: {}", isHeartrateEnabled_);
        TRY_EXECUTE({ deviceMonitor_->enableHeartbeat(); });
    }
}
}  // namespace libobsensor
