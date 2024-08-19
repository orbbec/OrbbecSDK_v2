// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#include "LinuxUsbPal.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#if defined(BUILD_USB_PAL)
#include "usb/hid/HidDevicePort.hpp"
#include "usb/vendor/VendorUsbDevicePort.hpp"
#include "usb/uvc/ObLibuvcDevicePort.hpp"
#include "usb/uvc/ObV4lUvcDevicePort.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"
#endif

#if defined(BUILD_NET_PAL)
#include "ethernet/EthernetPal.hpp"
#endif

#include <cctype>  // std::tolower
#include <chrono>
#include <algorithm>

namespace libobsensor {

const std::vector<uint16_t> FemtoMegaDevPids = {
    0x0669,  // Femto Mega
    0x06c0,  // Femto Mega i
};

template <class T> static bool isMatchDeviceByPid(uint16_t pid, T &pids) {
    return std::any_of(pids.begin(), pids.end(), [pid](uint16_t pid_) { return pid_ == pid; });
}

#if defined(BUILD_USB_PAL)
int deviceArrivalCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
int deviceRemovedCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
class LibusbDeviceWatcher : public IDeviceWatcher {
public:
    LibusbDeviceWatcher() = default;
    ~LibusbDeviceWatcher() noexcept override {
        TRY_EXECUTE(stop());
        // libusb_exit();
    }
    void start(deviceChangedCallback callback) override {
        callback_ = callback;
        auto rc   = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0, 0x2BC5, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                                                     deviceArrivalCallback, this, &hp[0]);
        if(LIBUSB_SUCCESS != rc) {
            LOG_WARN("register libusb hotplug failed!");
        }
        rc = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, 0x2BC5, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                                              deviceRemovedCallback, this, &hp[1]);
        if(LIBUSB_SUCCESS != rc) {
            LOG_WARN("register libusb hotplug failed!");
        }
    }

    void stop() override {
        libusb_hotplug_deregister_callback(NULL, hp[0]);
        libusb_hotplug_deregister_callback(NULL, hp[1]);
    }

    static bool hasCapability() {
        return libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    }

private:
    libusb_hotplug_callback_handle hp[2];
    deviceChangedCallback          callback_;

    friend int deviceArrivalCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
    friend int deviceRemovedCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
};

std::string parseDevicePath(libusb_device *usbDevice) {
    auto usb_bus = std::to_string(libusb_get_bus_number(usbDevice));
    // As per the USB 3.0 specs, the current maximum limit for the depth is 7.
    const auto               max_usb_depth            = 8;
    uint8_t                  usb_ports[max_usb_depth] = {};
    std::stringstream        port_path;
    auto                     port_count = libusb_get_port_numbers(usbDevice, usb_ports, max_usb_depth);
    auto                     usb_dev    = std::to_string(libusb_get_device_address(usbDevice));
    libusb_device_descriptor dev_desc{};
    auto                     r = libusb_get_device_descriptor(usbDevice, &dev_desc);
    (void)r;
    for(int i = 0; i < port_count; ++i) {
        port_path << std::to_string(usb_ports[i]) << (((i + 1) < port_count) ? "." : "");
    }
    return usb_bus + "-" + port_path.str() + "-" + usb_dev;
}

int deviceArrivalCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
    (void)ctx;
    (void)event;
    auto watcher = (LibusbDeviceWatcher *)user_data;
    LOG_DEBUG("Device arrival event occurred");
    watcher->callback_(OB_DEVICE_ARRIVAL, parseDevicePath(device));
    return 0;
}

int deviceRemovedCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
    (void)ctx;
    (void)event;
    auto watcher = (LibusbDeviceWatcher *)user_data;
    LOG_DEBUG("Device removed event occurred");
    watcher->callback_(OB_DEVICE_REMOVED, parseDevicePath(device));
    return 0;
}
#endif

std::shared_ptr<IPal> createUsbPal() {
    return std::make_shared<LinuxUsbPal>();
}

LinuxUsbPal::LinuxUsbPal() {
#if defined(BUILD_USB_PAL)
    usbEnumerator_ = IUsbEnumerator::getInstance();
#endif
}

LinuxUsbPal::~LinuxUsbPal() noexcept {}

std::shared_ptr<ISourcePort> LinuxUsbPal::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    std::unique_lock<std::mutex> lock(sourcePortMapMutex_);
    std::shared_ptr<ISourcePort> port;

    // clear expired weak_ptr
    for(auto it = sourcePortMap_.begin(); it != sourcePortMap_.end();) {
        if(it->second.expired()) {
            it = sourcePortMap_.erase(it);
        }
        else {
            ++it;
        }
    }

    // check if the port already exists in the map
    for(const auto &pair: sourcePortMap_) {
        if(pair.first->equal(portInfo)) {
            port = pair.second.lock();
            if(port != nullptr) {
                return port;
            }
        }
    }

#if defined(BUILD_USB_PAL)
    loadXmlConfig();
#endif

    switch(portInfo->portType) {
#if defined(BUILD_USB_PAL)
    case SOURCE_PORT_USB_VENDOR: {
        auto usbDev = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<VendorUsbDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }
    case SOURCE_PORT_USB_UVC: {
        auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
        auto backend     = uvcBackendType_;
        if(isMatchDeviceByPid(usbPortInfo->pid, FemtoMegaDevPids)) {  // if the device is femto mega, force to use v4l2
            backend = UVC_BACKEND_TYPE_V4L2;
        }
        if(backend == UVC_BACKEND_TYPE_AUTO) {
            backend = UVC_BACKEND_TYPE_LIBUVC;
            if(ObV4lUvcDevicePort::isContainedMetadataDevice(usbPortInfo)) {
                backend = UVC_BACKEND_TYPE_V4L2;
            }
        }
        if(backend == UVC_BACKEND_TYPE_V4L2) {
            port = std::make_shared<ObV4lUvcDevicePort>(usbPortInfo);
            LOG_DEBUG("UVC device have been create with V4L2 backend! dev: {}, inf: {}", usbPortInfo->url, usbPortInfo->infUrl);
        }
        else {
            auto usbDev = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
            if(usbDev == nullptr) {
                throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
            }
            port = std::make_shared<ObLibuvcDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
            LOG_DEBUG("UVC device have been create with LibUVC backend! dev: {}, inf: {}", usbPortInfo->url, usbPortInfo->infUrl);
        }
        break;
    }
    case SOURCE_PORT_USB_HID: {
        auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
        auto usbDev      = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<HidDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }
#endif

#if defined(BUILD_NET_PAL)
    case SOURCE_PORT_NET_VENDOR:
        port = std::make_shared<VendorNetDataPort>(std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_NET_RTSP:
        port = std::make_shared<RTSPStreamPort>(std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo));
        break;
    case SOURCE_PORT_NET_VENDOR_STREAM:
        port = std::make_shared<NetDataStreamPort>(std::dynamic_pointer_cast<const NetDataStreamPortInfo>(portInfo));
        break;
#endif

    default:
        throw libobsensor::invalid_value_exception("unsupported source port type!");
        break;
    }
    if(port != nullptr) {
        sourcePortMap_.insert(std::make_pair(portInfo, port));
    }
    return port;
}

#if defined(BUILD_USB_PAL)
std::shared_ptr<IDeviceWatcher> LinuxUsbPal::createDeviceWatcher() const {
    LOG_INFO("Create PollingDeviceWatcher!");

    if(LibusbDeviceWatcher::hasCapability()) {
        return std::make_shared<LibusbDeviceWatcher>();
    }
    LOG_WARN("Libusb is not available, return nullptr!");
    return nullptr;
}

SourcePortInfoList LinuxUsbPal::querySourcePortInfos() {
    SourcePortInfoList portInfoList;

    const auto &interfaceInfoList = usbEnumerator_->queryUsbInterfaces();
    for(const auto &info: interfaceInfoList) {
        auto portInfo      = std::make_shared<USBSourcePortInfo>(cvtUsbClassToPortType(info.cls));
        portInfo->url      = info.url;
        portInfo->uid      = info.uid;
        portInfo->vid      = info.vid;
        portInfo->pid      = info.pid;
        portInfo->serial   = info.serial;
        portInfo->connSpec = usbSpecToString(static_cast<UsbSpec>(info.conn_spec));
        portInfo->infUrl   = info.infUrl;
        portInfo->infIndex = info.infIndex;
        portInfo->infName  = info.infName;
        portInfo->hubId    = info.hubId;

        portInfoList.push_back(portInfo);
    }
    return portInfoList;
}

#if defined(BUILD_USB_PAL)
void LinuxUsbPal::loadXmlConfig() {
    // FIXME
    // auto ctx       = Context::getInstance();
    //    auto xmlConfig = ctx->getXmlConfig();
    //    if(!xmlConfig->isLoadConfigFileSuccessful()) {
    //        return;
    //    }
    //
    //    if(xmlConfig->isNodeContained("Device.LinuxUVCBackend")) {
    //        std::string backend = "";
    //        if(xmlConfig->getStringValue("Device.LinuxUVCBackend", backend)) {
    //            if(backend == "V4L2") {
    //                uvcBackendType_ = UVC_BACKEND_TYPE_V4L2;
    //            }
    //            else if(backend == "LibUVC") {
    //                uvcBackendType_ = UVC_BACKEND_TYPE_LIBUVC;
    //            }
    //            else {
    //                uvcBackendType_ = UVC_BACKEND_TYPE_AUTO;
    //            }
    //        }
    //        LOG_DEBUG("Uvc backend have been set to {}({})", backend, uvcBackendType_);
    //    }
    //    else if(xmlConfig->isNodeContained("Enumeration.Mode")) {  // deprecated, for compatibility
    //        std::string mode = "";
    //        if(xmlConfig->getStringValue("Enumeration.Mode", mode)) {
    //            if(mode == "V4L2") {
    //                uvcBackendType_ = UVC_BACKEND_TYPE_V4L2;
    //            }
    //            else {
    //                uvcBackendType_ = UVC_BACKEND_TYPE_LIBUVC;
    //            }
    //        }
    //        LOG_DEBUG("Uvc backend have been set to {}({})", mode, uvcBackendType_);
    //    }
}
#endif

#endif

}  // namespace libobsensor
