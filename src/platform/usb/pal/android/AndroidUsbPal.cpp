// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AndroidUsbPal.hpp"

#include <usb/hid/HidDevicePort.hpp>
#include <usb/pal/android/AndroidUsbDeviceManager.hpp>

#include "usb/enumerator/UsbTypes.hpp"
#include "usb/uvc/ObLibuvcDevicePort.hpp"
#include "usb/uvc/ObV4lUvcDevicePort.hpp"
#include "usb/vendor/VendorUsbDevicePort.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include "ethernet/EthernetPal.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {

std::shared_ptr<IPal> createUsbPal() {
    return std::make_shared<AndroidUsbPal>();
}

AndroidUsbPal::AndroidUsbPal() {
    androidUsbManager_ = AndroidUsbDeviceManager::getInstance();
    loadXmlConfig();
    LOG_DEBUG("Create AndroidUsbPal!");
}
AndroidUsbPal::~AndroidUsbPal() noexcept {
    androidUsbManager_ = nullptr;
    LOG_DEBUG("Destructor of AndroidUsbPal");
};

std::shared_ptr<ISourcePort> AndroidUsbPal::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    LOG_DEBUG(">>Create source port for");
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

    switch(portInfo->portType) {
    case SOURCE_PORT_USB_VENDOR: {
        auto usbDev = androidUsbManager_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<VendorUsbDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }
    case SOURCE_PORT_USB_UVC: {
        auto backend     = uvcBackendType_;
        if(backend == OB_UVC_BACKEND_TYPE_V4L2) {
            auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
            port = std::make_shared<ObV4lUvcDevicePort>(usbPortInfo);
            LOG_DEBUG("UVC device have been create with V4L2 backend! dev: {}, inf: {}", usbPortInfo->url, usbPortInfo->infUrl);
        } else {
            auto usbDev = androidUsbManager_->openUsbDevice(
                    std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
            if (usbDev == nullptr) {
                throw libobsensor::camera_disconnected_exception(
                        "usbEnumerator openUsbDevice failed!");
            }
            LOG_DEBUG("Create ObLibuvcDevicePort!");
            port = std::make_shared<ObLibuvcDevicePort>(usbDev,
                                                        std::dynamic_pointer_cast<const USBSourcePortInfo>(
                                                                portInfo));
        }
        break;
    }
    case SOURCE_PORT_USB_HID: {
        auto usbDev = androidUsbManager_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<HidDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }

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

std::shared_ptr<ISourcePort> AndroidUsbPal::getUvcSourcePort(std::shared_ptr<const SourcePortInfo> portInfo, OBUvcBackendType backendHint){
    if(portInfo->portType != SOURCE_PORT_USB_UVC) {
        throw libobsensor::invalid_value_exception("unsupported source port type!");
    }

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

    auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
    auto backend     = uvcBackendType_;
    if(backend == OB_UVC_BACKEND_TYPE_AUTO) {
        backend = backendHint;
        if(backend == OB_UVC_BACKEND_TYPE_AUTO && ObV4lUvcDevicePort::isContainedMetadataDevice(usbPortInfo)) {
            backend = OB_UVC_BACKEND_TYPE_V4L2;
        }
        else if(backend == OB_UVC_BACKEND_TYPE_V4L2 && !ObV4lUvcDevicePort::isContainedMetadataDevice(usbPortInfo)) {
            backend = OB_UVC_BACKEND_TYPE_LIBUVC;
        }
    }

    if(backend == OB_UVC_BACKEND_TYPE_V4L2) {
        port = std::make_shared<ObV4lUvcDevicePort>(usbPortInfo);
        LOG_DEBUG("UVC device have been create with V4L2 backend! dev: {}, inf: {}", usbPortInfo->url, usbPortInfo->infUrl);
    }
    else {
        auto usbDev = androidUsbManager_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        LOG_DEBUG("Create ObLibuvcDevicePort!");
        port = std::make_shared<ObLibuvcDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        LOG_DEBUG("UVC device have been create with LibUVC backend! dev: {}, inf: {}", usbPortInfo->url, usbPortInfo->infUrl);
    }

    if(port != nullptr) {
        sourcePortMap_.insert(std::make_pair(portInfo, port));
    }
    return port;
}

void AndroidUsbPal::setUvcBackendType(OBUvcBackendType backendType) {
    uvcBackendType_ = backendType;
}

std::shared_ptr<IDeviceWatcher> AndroidUsbPal::createDeviceWatcher() const {
    LOG_INFO("Create AndroidUsbDeviceManager!");
    return AndroidUsbDeviceManager::getInstance();
}

SourcePortInfoList AndroidUsbPal::querySourcePortInfos() {
    SourcePortInfoList portInfoList;

    const auto &usbInfoList = androidUsbManager_->getDeviceInfoList();
    for(const auto &info: usbInfoList) {
        auto portInfo      = std::make_shared<USBSourcePortInfo>(cvtUsbClassToPortType(info.cls));
        portInfo->url      = info.url;
        portInfo->uid      = info.uid;
        portInfo->vid      = info.vid;
        portInfo->pid      = info.pid;
        portInfo->serial   = info.serial;
        portInfo->connSpec = usb_spec_names.find(static_cast<UsbSpec>(info.conn_spec))->second;
        portInfo->infUrl   = info.infUrl;
        portInfo->infIndex = info.infIndex;
        portInfo->infName  = info.infName;
        // portInfo->hubId    = info.hubId;

        portInfoList.push_back(portInfo);
    }
    return portInfoList;
}

void AndroidUsbPal::loadXmlConfig() {
    auto        envConfig = EnvConfig::getInstance();
    std::string backend   = "";

    if (envConfig->getStringValue("Device.LinuxUVCBackend", backend)) {
        if (backend == "V4L2") {
            uvcBackendType_ = OB_UVC_BACKEND_TYPE_V4L2;
        }
        else if (backend == "LIBUVC") {
            uvcBackendType_ = OB_UVC_BACKEND_TYPE_LIBUVC;
        }
        else {
            uvcBackendType_ = OB_UVC_BACKEND_TYPE_AUTO;
        }
    } else {
        uvcBackendType_ = OB_UVC_BACKEND_TYPE_AUTO;
    }
    LOG_DEBUG("Uvc backend have been set to {}", static_cast<int>(uvcBackendType_));
}

}  // namespace libobsensor