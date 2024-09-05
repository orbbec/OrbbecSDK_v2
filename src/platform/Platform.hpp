// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#pragma once

#include "ISourcePort.hpp"
#include "IDeviceWatcher.hpp"
#include "IPal.hpp"

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
