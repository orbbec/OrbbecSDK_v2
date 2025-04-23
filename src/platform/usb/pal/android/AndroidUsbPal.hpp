// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"

#include <memory>
#include <mutex>
#include <map>

#if defined(BUILD_USB_PAL)
#include "usb/pal/android/AndroidUsbDeviceManager.hpp"
#endif

namespace libobsensor {

class AndroidUsbPal : public IPal {
public:
    AndroidUsbPal();
    virtual ~AndroidUsbPal() noexcept override;

    virtual std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    std::shared_ptr<ISourcePort> getUvcSourcePort(std::shared_ptr<const SourcePortInfo> portInfo, OBUvcBackendType backendHint);
    void                         setUvcBackendType(OBUvcBackendType backendType);

    virtual std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    virtual SourcePortInfoList              querySourcePortInfos() override;

    std::shared_ptr<IDeviceWatcher> getAndroidUsbManager() {
        return androidUsbManager_;
    }

private:
    std::shared_ptr<AndroidUsbDeviceManager>                                    androidUsbManager_;
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
    OBUvcBackendType uvcBackendType_ = OB_UVC_BACKEND_TYPE_LIBUVC;

private:
    void loadXmlConfig();
};

}  // namespace libobsensor

