// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#ifndef __ANDROID__

#include "UsbEnumeratorLibusb.hpp"

#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

std::string getDevicePath(libusb_device *device) {
#ifdef WIN32
    auto path = libusb_get_windows_path(device);
    if(!path) {
        return "";
    }
    return path;
#else
    auto usb_bus = std::to_string(libusb_get_bus_number(device));

    // As per the USB 3.0 specs, the current maximum limit for the depth is 7.
    const auto        max_usb_depth            = 8;
    uint8_t           usb_ports[max_usb_depth] = {};
    std::stringstream port_path;
    auto              port_count = libusb_get_port_numbers(device, usb_ports, max_usb_depth);
    auto              usb_dev    = std::to_string(libusb_get_device_address(device));

    for(int i = 0; i < port_count; ++i) {
        port_path << std::to_string(usb_ports[i]) << (((i + 1) < port_count) ? "." : "");
    }

    return usb_bus + "-" + port_path.str() + "-" + usb_dev;
#endif
}

std::string getDeviceHubID(const std::string &devicePath) {
    auto sstart = devicePath.find_first_of('-');
    auto send   = devicePath.find_last_of('-');
    if(sstart == std::string::npos || send == std::string::npos || sstart + 1 > send - 1) {
        return devicePath.substr(0, sstart);
    }
    std::string object = devicePath.substr(sstart + 1, send - sstart - 1);
    auto        ssend  = object.find_last_of('.');
    if(ssend == std::string::npos) {
        return devicePath.substr(0, sstart);
    }
    else {
        return devicePath.substr(0, sstart) + "-" + object.substr(0, ssend);
    }
}

#ifdef WIN32
/*
 *  	0x01516fe8 "\\\\?\\USB#VID_2BC5&PID_0633#6&3436DA6F&1&2#{A5DCBF10-6530-11D2-901F-00C04FB951ED}"
 *    0x0152e920 "\\\\?\\USB#VID_2BC5&PID_0532#USB#{A5DCBF10-6530-11D2-901F-00C04FB951ED}"
 */
// Parse the following USB path format = \?usb#vid_vvvv&pid_pppp&mi_ii#aaaaaaaaaaaaaaaa#{gggggggg-gggg-gggg-gggg-gggggggggggg}
// vvvv = USB vendor ID represented in 4 hexadecimal characters.
// pppp = USB product ID represented in 4 hexadecimal characters.
// ii = USB interface number.
// aaaaaaaaaaaaaaaa = unique Windows-generated string based on things such as the physical USB port address and/or interface number.
// gggggggg-gggg-gggg-gggg-gggggggggggg = device interface GUID assigned in the driver or driver INF file and is used to link applications to device with
// specific drivers loaded.
std::string getDeviceUidByWin(const std::string &devicePath) {
    auto name   = utils::string::toLower(devicePath);
    auto tokens = utils::string::tokenize(name, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return "";  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return "";
    }

    auto ids = utils::string::tokenize(tokens[2], '&');
    if(ids.empty()) {
        LOG_ERROR("malformed id string: {}", tokens[2]);
        return "";
    }

    if(ids.size() == 1) {
        std::string TempId = tokens[2];
        if(TempId.length() >= 11) {
            return tokens[2];
        }
    }

    if(ids.size() > 2) {
        // uid = ids[1];
        return utils::string::remove(tokens[2], "&");
    }

    return "";
}

std::string getDeviceUidByWinDeviceID(const std::string &deviceID) {
    auto name   = utils::string::toLower(deviceID);
    auto tokens = utils::string::tokenize(name, '\\');
    if(tokens.empty() || tokens[0] != R"(usb)")
        return "";  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return "";
    }

    auto ids = utils::string::tokenize(tokens[2], '&');
    if(ids.empty()) {
        LOG_ERROR("malformed id string: {}", tokens[2]);
        return "";
    }

    if(ids.size() > 2)
        return ids[1];
    return "";
}

std::string getDeviceHubIdByWin(const std::string &devicePath) {
    auto name   = utils::string::toLower(devicePath);
    auto tokens = utils::string::tokenize(name, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return "";  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return "";
    }
    return tokens[2];
}
#endif

bool findSN2Toupper(const std::string &src, std::string &dst) {
    auto name   = utils::string::toLower(src);
    auto tokens = utils::string::tokenize(name, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        return false;
    }

    if(tokens[2].find("&") != std::string::npos) {
        return false;
    }

    if(tokens[2].length() < 11) {
        return false;
    }

    dst = utils::string::toUpper(tokens[2]);
    return true;
}

std::string getStringDesc(libusb_device_handle *deviceHandle, uint8_t descIndex) {
    if(descIndex == 0) {
        throw invalid_value_exception("Invalid descriptor index");
    }

    char str[256];
    auto rst = libusb_get_string_descriptor_ascii(deviceHandle, descIndex, reinterpret_cast<unsigned char *>(str), 256);  // get length
    if(rst < 0) {
        LOG_ERROR("Failed to get string descriptor: error={}", libusb_strerror(rst));
        return "";
    }
    str[rst] = '\0';
    return std::string(str);
}

// Get the sub-devices of a composite device.
std::vector<UsbInterfaceInfo> queryInterfaces(libusb_device *device, libusb_device_descriptor desc) {
#ifdef WIN32
    std::string    hub_id;
    libusb_device *parent_dev = libusb_get_parent(device);
    if(parent_dev != nullptr) {
        auto hub_dev_path = libusb_get_windows_path(parent_dev);
        if(hub_dev_path) {
            hub_id = getDeviceHubIdByWin(hub_dev_path);
        }
        // LOG_ERROR("hubid = " << hub_id;
    }
#endif

    std::vector<UsbInterfaceInfo> rv;
    for(uint8_t c = 0; c < desc.bNumConfigurations; ++c) {
        libusb_config_descriptor *config = nullptr;
        auto                      ret    = libusb_get_config_descriptor(device, c, &config);
        if(LIBUSB_SUCCESS != ret) {
            LOG_WARN("Failed to read USB config descriptor: error={}", ret);
            continue;
        }

        for(uint8_t i = 0; i < config->bNumInterfaces; ++i) {
            auto inf = config->interface[i];

            if(inf.altsetting->bInterfaceClass == LIBUSB_CLASS_VIDEO && inf.altsetting->bInterfaceSubClass == 2) {
                // 2 is for streaming interface class of video class
                continue;
            }

            if(inf.altsetting->bInterfaceClass == LIBUSB_CLASS_APPLICATION || inf.altsetting->bInterfaceClass == LIBUSB_CLASS_HUB) {
                continue;
            }

            UsbInterfaceInfo info{};
            info.url = getDevicePath(device);
            if(info.url.empty()) {
                continue;
            }
            info.cls = inf.altsetting->bInterfaceClass;

#ifdef WIN32
            if(info.cls == LIBUSB_CLASS_HID) {
                auto sub_composite_device_id = libusb_get_sub_composite_device_id(device, i);
                if(sub_composite_device_id != nullptr) {
                    info.uid = getDeviceUidByWinDeviceID(sub_composite_device_id);
                }
                auto interface_path = libusb_get_interface_path(device, i);
                if(interface_path != nullptr) {
                    info.infUrl = interface_path;
                }
            }
            else {
                auto interface_path = libusb_get_interface_path(device, i);
                if(interface_path != nullptr) {
                    info.infUrl = interface_path;
                    info.uid    = getDeviceUidByWin(interface_path);
                }
                else if(config->bNumInterfaces == 1) {  // Non-composite equipment
                    auto dev_path = libusb_get_windows_path(device);
                    info.infUrl   = dev_path;
                    if(dev_path != nullptr) {
                        info.uid = getDeviceUidByWin(dev_path);
                    }
                }
            }
            info.hubId = hub_id;
#else
            info.infUrl = info.url + "." + std::to_string(i);

            info.uid   = info.url;
            info.hubId = getDeviceHubID(info.url);
#endif

            info.conn_spec        = static_cast<UsbSpec>(desc.bcdUSB);
            info.vid              = desc.idVendor;
            info.pid              = desc.idProduct;
            info.infIndex         = inf.altsetting->bInterfaceNumber;
            info.infNameDescIndex = inf.altsetting->iInterface;

            rv.push_back(info);
        }

        libusb_free_config_descriptor(config);
    }
    return rv;
}

UsbDeviceLibusb::UsbDeviceLibusb(libusb_context *libusbCtx, std::shared_ptr<libusb_device_handle> handle) : libusbCtx_(libusbCtx), handle_(handle) {}

libusb_device_handle *UsbDeviceLibusb::getLibusbDeviceHandle() const {
    return handle_.get();
}

libusb_context *UsbDeviceLibusb::getLibusbContext() const {
    return libusbCtx_;
}

libusb_endpoint_descriptor UsbDeviceLibusb::getEndpointDesc(int interfaceIndex, libusb_endpoint_transfer_type transferType,
                                                            libusb_endpoint_direction direction) const {
    libusb_device_handle    *handle = getLibusbDeviceHandle();
    libusb_device           *device = libusb_get_device(handle);
    libusb_device_descriptor desc;
    auto                     ret = libusb_get_device_descriptor(device, &desc);
    if(ret != LIBUSB_SUCCESS) {
        throw io_exception(utils::string::to_string() << "Failed to read USB device descriptor: error=" << libusb_strerror(ret));
    }

    libusb_endpoint_descriptor ep    = { 0 };
    bool                       found = false;
    for(uint8_t c = 0; c < desc.bNumConfigurations; ++c) {
        libusb_config_descriptor *config = nullptr;
        ret                              = libusb_get_config_descriptor(device, c, &config);
        if(ret != LIBUSB_SUCCESS) {
            LOG_WARN("Failed to read USB config descriptor: error={}", libusb_strerror(ret));
        }

        if(config->bNumInterfaces <= interfaceIndex) {
            continue;
        }

        auto inf = config->interface[interfaceIndex].altsetting;

        for(uint8_t i = 0; i < inf->bNumEndpoints; ++i) {
            ep = inf->endpoint[i];
            if((ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == transferType && (ep.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == direction) {
                found = true;
                break;
            }
        }
        libusb_free_config_descriptor(config);
    }
    if(!found) {
        throw io_exception("Can not find interface to get endpoint address");
    }
    return ep;
}

std::weak_ptr<IUsbEnumerator> IUsbEnumerator::instanceWeakPtr_;
std::mutex                    IUsbEnumerator::instanceMutex_;

std::shared_ptr<IUsbEnumerator> IUsbEnumerator::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<UsbEnumeratorLibusb>(new UsbEnumeratorLibusb());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

UsbEnumeratorLibusb::UsbEnumeratorLibusb() {
#ifdef __ANDROID__
    auto rc = libusb_set_option(ctx_, LIBUSB_OPTION_WEAK_AUTHORITY, NULL);
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
    LOG_DEBUG("UsbEnumeratorLibusb created");
}

UsbEnumeratorLibusb::~UsbEnumeratorLibusb() noexcept {
    stopEventHandleThread();
    libusb_exit(libusbCtx_);
    LOG_DEBUG("UsbEnumeratorLibusb destroyed");
}

const std::vector<UsbInterfaceInfo> &UsbEnumeratorLibusb::queryUsbInterfaces() {
    std::vector<UsbInterfaceInfo> tempInfoList;
    libusb_device               **devList;
    auto                          count = libusb_get_device_list(libusbCtx_, &devList);
    for(ssize_t i = 0; i < count; ++i) {
        auto                     device = devList[i];
        libusb_device_descriptor desc{};
        auto                     ret = libusb_get_device_descriptor(device, &desc);
        if(ret != LIBUSB_SUCCESS) {
            LOG_DEBUG("Failed to read USB device descriptor: error={}", libusb_strerror(ret));
            continue;
        }

        if(desc.idVendor != ORBBEC_USB_VID) {  // filter out non-orbbec devices
            continue;
        }

        // todo: remove interface info from devInterfaceList_ when device is removed
        auto path  = getDevicePath(device);
        bool found = false;
        for(auto devInfoIter = devInterfaceList_.begin(); devInfoIter != devInterfaceList_.end();) {
            if(devInfoIter->url == path) {
                tempInfoList.push_back(*devInfoIter);
                found = true;
            }
            ++devInfoIter;
        }
        if(found) {
            continue;
        }

        libusb_device_handle *handle = nullptr;
        auto                  rst    = libusb_open(device, &handle);
        if(rst != LIBUSB_SUCCESS) {
            LOG_WARN("Failed to open USB device: error={}", libusb_strerror(rst));
            continue;
        }

        std::string serial;
        try {
            serial = getStringDesc(handle, desc.iSerialNumber);
        }
        catch(const std::exception &e) {
            LOG_ERROR("Failed to query USB device serial number: {}", e.what());
            serial = "";
        }

        auto infs = queryInterfaces(device, desc);
        for(auto &inf: infs) {
#ifdef WIN32
            if(serial.empty()) {
                std::string toupperSNStr;
                if(findSN2Toupper(inf.url, toupperSNStr)) {
                    serial = toupperSNStr;
                }
            }
#endif
            inf.serial  = serial;
            inf.infName = getStringDesc(handle, inf.infNameDescIndex);
            tempInfoList.push_back(inf);
        }

        libusb_close(handle);
    }

    libusb_free_device_list(devList, 1);

    LOG_DEBUG("queryUsbInterfaces done!");

    devInterfaceList_ = tempInfoList;
    return devInterfaceList_;
}

std::shared_ptr<IUsbDevice> UsbEnumeratorLibusb::openUsbDevice(const std::string &devUrl) {
    uint8_t retry = 1;
    do {
        std::shared_ptr<libusb_device_handle> devHandle;
        BEGIN_TRY_EXECUTE({ devHandle = openLibusbDevice(devUrl); })
        CATCH_EXCEPTION_AND_EXECUTE({ continue; })
        auto dev = std::make_shared<UsbDeviceLibusb>(libusbCtx_, devHandle);
        return dev;
    } while(retry--);

    if(retry == 0) {
        throw io_exception(utils::string::to_string() << "Can not open device: " << devUrl);
    }

    return nullptr;
}

void UsbEnumeratorLibusb::startEventHandleThread() {
    LOG_DEBUG("UsbContext::startEventHandler()");
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

void UsbEnumeratorLibusb::stopEventHandleThread() {
    LOG_DEBUG("UsbContext::stopEventHandler()");
    libusbEventHandlerExit_ = 1;
    libusb_interrupt_event_handler(libusbCtx_);
    if(libusbEventHandlerThread_.joinable()) {
        libusbEventHandlerThread_.join();
    }
}

std::shared_ptr<libusb_device_handle> UsbEnumeratorLibusb::openLibusbDevice(const std::string &devUrl) {
    std::shared_ptr<libusb_device_handle> devHandle;
    std::unique_lock<std::mutex>          lock(libusbDeviceHandleMutex_);

    // check expired weak_ptr
    for(auto iter = libusbDeviceHandles_.begin(); iter != libusbDeviceHandles_.end();) {
        auto handle = iter->second.lock();
        if(!handle) {
            iter = libusbDeviceHandles_.erase(iter);
            continue;
        }
        if(iter->first == devUrl) {
            devHandle = handle;
            // break; // do not break, continue to check and clear expired weak_ptr
        }
        iter++;
    }

    if(devHandle) {
        return devHandle;
    }

    // open device handle

    libusb_device **devList = nullptr;
    auto            count   = libusb_get_device_list(libusbCtx_, &devList);
    if(count <= 0) {
        throw invalid_value_exception(utils::string::to_string() << "Get device list return 0, can not open device: " << devUrl);
    }

    for(int i = 0; i < count; i++) {
        libusb_device *device = devList[i];
        auto           path   = getDevicePath(device);
        if(path == devUrl) {
            libusb_device_handle *handle = nullptr;
            auto                  rst    = libusb_open(device, &handle);
            if(rst != LIBUSB_SUCCESS) {
                libusb_free_device_list(devList, 1);

                throw invalid_value_exception(utils::string::to_string() << "Open device failed: " << libusb_strerror(rst));
            }

            devHandle = std::shared_ptr<libusb_device_handle>(handle, [this, devUrl](libusb_device_handle *handle) {
                std::unique_lock<std::mutex> lock(libusbDeviceHandleMutex_);
                libusb_close(handle);
                libusbDeviceHandles_.erase(devUrl);
            });

            libusbDeviceHandles_[devUrl] = devHandle;
            break;
        }
    }

    libusb_free_device_list(devList, 1);

    if(!devHandle) {
        throw invalid_value_exception(utils::string::to_string() << "Can not open device: " << devUrl);
    }

    return devHandle;
}

}  // namespace libobsensor
#endif  // __ANDROID__
