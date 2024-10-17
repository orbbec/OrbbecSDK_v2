// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include <iostream>
#include <vector>
#include <map>

#include "usb/enumerator/IUsbEnumerator.hpp"
namespace libobsensor {

class MacUsbPal : public IPal {
public:
    MacUsbPal();
    ~MacUsbPal() noexcept override;

    std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;

public:
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    SourcePortInfoList              querySourcePortInfos() override;

private:
    std::shared_ptr<IUsbEnumerator> usbEnumerator_;
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor

