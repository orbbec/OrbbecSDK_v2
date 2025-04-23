// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UsbContext.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"

namespace libobsensor {
UsbContext::UsbContext() {
    libusbEventHandlerExit_ = 0;
    libusbCtx_             = nullptr;
#ifdef __ANDROID__
    auto rc = libusb_set_option(libusbCtx_, LIBUSB_OPTION_WEAK_AUTHORITY, NULL);
    if(rc != LIBUSB_SUCCESS) {
        LOG_ERROR("libusb set option LIBUSB_OPTION_WEAK_AUTHORITY failed!");
        throw std::runtime_error("libusb set option LIBUSB_OPTION_WEAK_AUTHORITY failed");
    }
#endif

    auto sts = libusb_init(&libusbCtx_);
    if(sts != LIBUSB_SUCCESS) {
        LOG_ERROR("libusb_init failed");
    }

    startEventHandleThread();

    LOG_DEBUG("UsbContext created");
}

UsbContext::~UsbContext() {
    stopEventHandleThread();
    libusb_exit(libusbCtx_);
    LOG_DEBUG("UsbContext destroyed");
}

libusb_context *UsbContext::getContext() const {
    return libusbCtx_;
}

void UsbContext::startEventHandleThread() {
    LOG_DEBUG("startEventHandler ...");
    libusbEventHandlerExit_   = 0;
    libusbEventHandlerThread_ = std::thread([&]() {
        while(!libusbEventHandlerExit_) {
            auto rc = libusb_handle_events_completed(libusbCtx_, &libusbEventHandlerExit_);
            if(rc != LIBUSB_SUCCESS) {
                LOG_WARN_INTVL("libusb_handle_events_completed failed: {}", libusb_strerror(rc));
            }
        }
    });
}

void UsbContext::stopEventHandleThread() {
    LOG_DEBUG("stopEventHandler ...");
    libusbEventHandlerExit_ = 1;
    libusb_interrupt_event_handler(libusbCtx_);
    if(libusbEventHandlerThread_.joinable()) {
        libusbEventHandlerThread_.join();
    }
}

} // namespace liborsensor
