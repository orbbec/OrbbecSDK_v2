// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "component/DeviceComponentBase.hpp"
#include "component/timestamp/GlobalTimestampFitter.hpp"
#include "component/monitor/DeviceMonitor.hpp"

#include <memory>

namespace libobsensor {
class IFirmwareUpdateGuard;

// FirmwareUpdateGuardFactory
class FirmwareUpdateGuardFactory : public DeviceComponentBase {
public:
    FirmwareUpdateGuardFactory(IDevice *owner);

    std::shared_ptr<IFirmwareUpdateGuard> create();
};

// IFirmwareUpdateGuard
class IFirmwareUpdateGuard {
public:
    virtual ~IFirmwareUpdateGuard() = default;

    virtual void preUpdate()  = 0;
    virtual void postUpdate() = 0;
};

// CompositeGuard
class CompositeGuard : public IFirmwareUpdateGuard {
public:
    explicit CompositeGuard() = default;
    explicit CompositeGuard(const std::vector<std::shared_ptr<IFirmwareUpdateGuard>> &guards);
    virtual ~CompositeGuard() noexcept override;

    CompositeGuard(CompositeGuard &&other) noexcept;
    CompositeGuard &operator=(CompositeGuard &&other) noexcept;

    CompositeGuard(const CompositeGuard &other)            = delete;
    CompositeGuard &operator=(const CompositeGuard &other) = delete;

    virtual void addGuard(std::shared_ptr<IFirmwareUpdateGuard> guard);
    virtual void preUpdate() override;
    virtual void postUpdate() override;

protected:
    std::vector<std::shared_ptr<IFirmwareUpdateGuard>> guards_;
};

// SeparateGuard
class NullGuard : public IFirmwareUpdateGuard {
public:
    explicit NullGuard(IDevice *owner);
    virtual ~NullGuard() noexcept override = default;

    virtual void preUpdate() override;
    virtual void postUpdate() override;
};

class GlobalTimestampGuard : public IFirmwareUpdateGuard {
public:
    explicit GlobalTimestampGuard(IDevice *owner);
    virtual ~GlobalTimestampGuard() noexcept override = default;

    GlobalTimestampGuard(GlobalTimestampGuard &&other) noexcept;
    GlobalTimestampGuard &operator=(GlobalTimestampGuard &&other) noexcept;

    GlobalTimestampGuard(const GlobalTimestampGuard &other)            = delete;
    GlobalTimestampGuard &operator=(const GlobalTimestampGuard &other) = delete;

    virtual void preUpdate() override;
    virtual void postUpdate() override;

protected:
    IDevice *owner_;
    bool     isGlobalTimestampEnabled_;

    std::shared_ptr<GlobalTimestampFitter> globalTimestampFilter_;
};

class HeardbeatGuard : public IFirmwareUpdateGuard {
public:
    explicit HeardbeatGuard(IDevice *owner);
    virtual ~HeardbeatGuard() noexcept override = default;

    HeardbeatGuard(HeardbeatGuard &&other) noexcept;
    HeardbeatGuard &operator=(HeardbeatGuard &&other) noexcept;

    HeardbeatGuard(const HeardbeatGuard &other)            = delete;
    HeardbeatGuard &operator=(const HeardbeatGuard &other) = delete;

    virtual void preUpdate() override;
    virtual void postUpdate() override;

protected:
    IDevice *owner_;
    bool     isHeartrateEnabled_;

    std::shared_ptr<DeviceMonitor> deviceMonitor_;
};
}  // namespace libobsensor
