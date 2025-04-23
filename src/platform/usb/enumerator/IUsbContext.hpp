// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libusb.h>

namespace libobsensor {
class IUsbContext {
public:
    virtual ~IUsbContext() = default;

    virtual libusb_context *getContext() const = 0;

    virtual void startEventHandleThread() = 0;
    virtual void stopEventHandleThread()  = 0;
};

}  // namespace libobsensor
