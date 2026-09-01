// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IPal.hpp"
#include <map>
#include <string>

#include "usb/enumerator/IUsbEnumerator.hpp"

namespace libobsensor {

class WinUsbPal : public IPal, public std::enable_shared_from_this<WinUsbPal> {
public:
    WinUsbPal();
    virtual ~WinUsbPal() noexcept override;

    std::shared_ptr<ISourcePort>    getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    SourcePortInfoList              querySourcePortInfos() override;
    void                            markUvcPortsDisconnected(uint16_t vid, uint16_t pid, const std::string &uid, const std::string &symbolicLink) const;

private:
    std::shared_ptr<IUsbEnumerator> usbEnumerator_;

private:
    mutable std::mutex                                                                  sourcePortMapMutex_;
    mutable std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor
