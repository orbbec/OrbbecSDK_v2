// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

#include <iostream>
#include <vector>
#include <map>

namespace libobsensor {
class LinuxUsbPal : public IPal {
public:
    LinuxUsbPal();
    ~LinuxUsbPal() noexcept override;

    std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    std::shared_ptr<ISourcePort> getUvcSourcePort(std::shared_ptr<const SourcePortInfo> portInfo, OBUvcBackendType backendHint);
    void                         setUvcBackendType(OBUvcBackendType backendType);

public:
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    SourcePortInfoList              querySourcePortInfos() override;

private:
    void loadXmlConfig();

    std::shared_ptr<IUsbEnumerator> usbEnumerator_;

    OBUvcBackendType uvcBackendType_ = OB_UVC_BACKEND_TYPE_LIBUVC;

private:
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor
