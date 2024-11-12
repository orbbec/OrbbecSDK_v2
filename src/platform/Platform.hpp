// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "IDeviceWatcher.hpp"
#include "IPal.hpp"

#if defined(__linux__)
#include "usb/uvc/UvcTypes.hpp"
#endif

#include <mutex>
#include <map>
#include <memory>

namespace libobsensor {

class Platform {
private:
    Platform();

    static std::mutex              instanceMutex_;
    static std::weak_ptr<Platform> instanceWeakPtr_;

public:
    static std::shared_ptr<Platform> getInstance();

    ~Platform() noexcept = default;

    std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo);

    SourcePortInfoList              queryUsbSourcePortInfos();
    std::shared_ptr<ISourcePort>    getUsbSourcePort(std::shared_ptr<const SourcePortInfo> portInfo);
    std::shared_ptr<IDeviceWatcher> createUsbDeviceWatcher() const;
#if defined(__linux__)
    std::shared_ptr<ISourcePort> getUvcSourcePort(std::shared_ptr<const SourcePortInfo> portInfo, OBUvcBackendType backendTypeHint);
    void                         setUvcBackendType(OBUvcBackendType backendType);
#endif

    SourcePortInfoList              queryNetSourcePort();
    std::shared_ptr<ISourcePort>    getNetSourcePort(std::shared_ptr<const SourcePortInfo> portInfo);
    std::shared_ptr<IDeviceWatcher> createNetDeviceWatcher();

    SourcePortInfoList              queryGmslSourcePort();
    std::shared_ptr<ISourcePort>    getGmslSourcePort(std::shared_ptr<const SourcePortInfo> portInfo);
    std::shared_ptr<IDeviceWatcher> createGmslDeviceWatcher();

private:
    std::map<std::string, std::shared_ptr<IPal>> palMap_;
};

}  // namespace libobsensor
