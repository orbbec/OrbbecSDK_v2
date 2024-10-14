// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"

#include <memory>

#if defined(BUILD_USB_PAL)
#include <pal/android/AndroidUsbDeviceManager.hpp>
#endif

namespace libobsensor {

class AndroidUsbPal : public IPal {
public:
    AndroidUsbPal();
    virtual ~AndroidUsbPal() noexcept;

    virtual std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;

#if defined(BUILD_USB_PAL)
    virtual std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    virtual SourcePortInfoList              querySourcePortInfos() override;
    virtual std::shared_ptr<ISourcePort>    createOpenNIDevicePort(std::shared_ptr<const SourcePortInfo>) override;
    virtual std::shared_ptr<ISourcePort>    createMultiUvcDevicePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    virtual std::shared_ptr<ISourcePort>    createRawPhaseConverterDevicePort(RawPhaseConverterPortType type, std::shared_ptr<const SourcePortInfo>) override;
#endif

    std::shared_ptr<AndroidUsbDeviceManager> getAndroidUsbManager() {
        return androidUsbManager_;
    }

private:
    std::shared_ptr<AndroidUsbDeviceManager>                                    androidUsbManager_;
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

}  // namespace libobsensor

