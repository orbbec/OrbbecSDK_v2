// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IUsbContext.hpp"

#include <thread>

namespace libobsensor {
class UsbContext : public IUsbContext {
public:
    UsbContext();
    virtual ~UsbContext() noexcept override;

    virtual libusb_context *getContext() const override;

    virtual void startEventHandleThread() override;
    virtual void stopEventHandleThread()  override;


private:
    libusb_context* libusbCtx_;
    std::thread     libusbEventHandlerThread_;
    int             libusbEventHandlerExit_;
};

}  // namespace libobsensor