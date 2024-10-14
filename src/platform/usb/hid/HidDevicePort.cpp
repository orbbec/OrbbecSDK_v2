// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HidDevicePort.hpp"

#include "frame/FrameFactory.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

namespace libobsensor {
HidDevicePort::HidDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo)
    : portInfo_(portInfo), usbDevice_(usbDevice), isStreaming_(false), frameQueue_(10) {

    auto libusbDevice = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
    auto epDesc       = libusbDevice->getEndpointDesc(portInfo->infIndex, LIBUSB_ENDPOINT_TRANSFER_TYPE_INTERRUPT, LIBUSB_ENDPOINT_IN);
    endpointAddress_  = epDesc.bEndpointAddress;
    maxPacketSize_    = epDesc.wMaxPacketSize;

    auto libusbDevHandle = libusbDevice->getLibusbDeviceHandle();
    if(libusb_kernel_driver_active(libusbDevHandle, portInfo->infIndex) == 1) {
        auto res = libusb_detach_kernel_driver(libusbDevHandle, portInfo->infIndex);
        if(res != LIBUSB_SUCCESS) {
            throw io_exception("detach kernel driver failed, error: " + std::string(libusb_strerror(res)));
        }
    }
    auto res = libusb_claim_interface(libusbDevHandle, portInfo->infIndex);
    if(res != LIBUSB_SUCCESS) {
        throw io_exception("claim interface failed, error: " + std::string(libusb_strerror(res)));
    }
    LOG_DEBUG("HidDevicePort::HidDevicePort done");
}

HidDevicePort::~HidDevicePort() noexcept {
    if(isStreaming_) {
        frameQueue_.stop();
        stopStream();
    }

    auto libusbDevice    = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
    auto libusbDevHandle = libusbDevice->getLibusbDeviceHandle();

    auto res = libusb_release_interface(libusbDevHandle, portInfo_->infIndex);
    if(res != LIBUSB_SUCCESS) {
        LOG_WARN("release interface failed, error: {}", libusb_strerror(res));
    }
    if(libusb_kernel_driver_active(libusbDevHandle, portInfo_->infIndex) == 1) {
        res = libusb_attach_kernel_driver(libusbDevHandle, portInfo_->infIndex);
        if(res != LIBUSB_SUCCESS) {
            LOG_WARN("attach kernel driver failed, error: {}", libusb_strerror(res));
        }
    }
    LOG_DEBUG("HidDevicePort::~HidDevicePort done");
}

void HidDevicePort::startStream(MutableFrameCallback callback) {
    if(isStreaming_) {
        throw wrong_api_call_sequence_exception("HidDevicePort::startStream() called while streaming");
    }
    isStreaming_ = true;
    frameQueue_.start(callback);

    streamThread_ = std::thread([this]() {
        auto libusbDevice    = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDevice_);
        auto libusbDevHandle = libusbDevice->getLibusbDeviceHandle();
        while(isStreaming_) {
            auto frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, maxPacketSize_);
            int  transferred;
            auto res = libusb_interrupt_transfer(libusbDevHandle, endpointAddress_, frame->getDataMutable(), static_cast<int>(frame->getDataSize()),
                                                 &transferred, 1000);
            if(res != LIBUSB_SUCCESS && isStreaming_) {
                LOG_WARN_INTVL(utils::string::to_string() << "interrupt transfer failed, error: " << libusb_strerror(res));
                continue;
            }
            auto realtime = utils::getNowTimesUs();
            frame->setSystemTimeStampUsec(realtime);
            frameQueue_.enqueue(frame);
        }
    });
    LOG_DEBUG("HidDevicePort::startStream done");
}

void HidDevicePort::stopStream() {
    if(!isStreaming_) {
        throw wrong_api_call_sequence_exception("HidDevicePort::stopStream() called while not streaming");
    }
    isStreaming_ = false;

    streamThread_.join();
    frameQueue_.flush();

    LOG_DEBUG("HidDevicePort::stopStream done");
}

std::shared_ptr<const SourcePortInfo> HidDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor

