// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#pragma once

#include "ISourcePort.hpp"
#include "IDeviceWatcher.hpp"

namespace libobsensor {

class IPal {
public:
    virtual ~IPal() noexcept = default;

    virtual SourcePortInfoList              querySourcePortInfos()                               = 0;
    virtual std::shared_ptr<ISourcePort>    getSourcePort(std::shared_ptr<const SourcePortInfo>) = 0;
    virtual std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const                          = 0;
};

#if defined(BUILD_USB_PAL)
std::shared_ptr<IPal> createUsbPal();
#endif

#if defined(BUILD_NET_PAL)
std::shared_ptr<IPal> createNetPal();
#endif
}  // namespace libobsensor