// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

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

class LinuxUsbPal : public IPal {
public:
    LinuxUsbPal();
    ~LinuxUsbPal() noexcept override;

    std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;

public:
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    SourcePortInfoList              querySourcePortInfos() override;

private:
    void loadXmlConfig();

    std::shared_ptr<IUsbEnumerator> usbEnumerator_;

    typedef enum {
        UVC_BACKEND_TYPE_AUTO,  // if support v4l2 metadata, use v4l2, else use libusb. default is auto
        UVC_BACKEND_TYPE_LIBUVC,
        UVC_BACKEND_TYPE_V4L2,
    } UvcBackendType;

    UvcBackendType uvcBackendType_ = UVC_BACKEND_TYPE_LIBUVC;

private:
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor
