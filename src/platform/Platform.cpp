#include "Platform.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

std::mutex              Platform::instanceMutex_;
std::weak_ptr<Platform> Platform::instanceWeakPtr_;

std::shared_ptr<Platform> Platform::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<Platform>(new Platform());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

Platform::Platform() {
#if defined(BUILD_USB_PAL)
    auto usbPal = createUsbPal();
    palMap_.insert(std::make_pair("usb", usbPal));
#endif

#if defined(BUILD_NET_PAL)
    auto netPal = createNetPal();
    palMap_.insert(std::make_pair("net", netPal));
#endif
}

std::shared_ptr<ISourcePort> Platform::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    if(IS_USB_PORT(portInfo->portType)) {
        return getUsbSourcePort(portInfo);
    }
    else if(IS_NET_PORT(portInfo->portType)) {
        return getNetSourcePort(portInfo);
    }
    else {
        throw pal_exception("Invalid port type!");
    }
}

std::shared_ptr<IDeviceWatcher> Platform::createUsbDeviceWatcher() const {
    auto pal = palMap_.find("usb");
    if(pal == palMap_.end()) {
        throw pal_exception("Usb pal is not exist, please check the build config that you have enabled BUILD_USB_PAL");
    }
    return pal->second->createDeviceWatcher();
}

SourcePortInfoList Platform::queryUsbSourcePortInfos() {
    auto pal = palMap_.find("usb");
    if(pal == palMap_.end()) {
        throw pal_exception("Usb pal is not exist, please check the build config that you have enabled BUILD_USB_PAL");
    }
    return pal->second->querySourcePortInfos();
}

std::shared_ptr<ISourcePort> Platform::getUsbSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    auto pal = palMap_.find("usb");
    if(pal == palMap_.end()) {
        throw pal_exception("Usb pal is not exist, please check the build config that you have enabled BUILD_USB_PAL");
    }
    return pal->second->getSourcePort(portInfo);
}

SourcePortInfoList Platform::queryNetSourcePort() {
    auto pal = palMap_.find("net");
    if(pal == palMap_.end()) {
        throw pal_exception("Net pal is not exist, please check the build config that you have enabled BUILD_NET_PAL");
    }
    return pal->second->querySourcePortInfos();
}

std::shared_ptr<IDeviceWatcher> Platform::createNetDeviceWatcher() {
    auto pal = palMap_.find("net");
    if(pal == palMap_.end()) {
        throw pal_exception("Net pal is not exist, please check the build config that you have enabled BUILD_NET_PAL");
    }
    return pal->second->createDeviceWatcher();
}

std::shared_ptr<ISourcePort> Platform::getNetSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    auto pal = palMap_.find("net");
    if(pal == palMap_.end()) {
        throw pal_exception("Net pal is not exist, please check the build config that you have enabled BUILD_NET_PAL");
    }
    return pal->second->getSourcePort(portInfo);
}

SourcePortInfoList Platform::queryGmslSourcePort() {
    auto pal = palMap_.find("gmsl");
    if(pal == palMap_.end()) {
        throw pal_exception("Gmsl pal is not exist, please check the build config that you have enabled BUILD_GMSL_PAL");
    }
    return pal->second->querySourcePortInfos();
}

std::shared_ptr<ISourcePort> Platform::getGmslSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    auto pal = palMap_.find("gmsl");
    if(pal == palMap_.end()) {
        throw pal_exception("Gmsl pal is not exist, please check the build config that you have enabled BUILD_GMSL_PAL");
    }
    return pal->second->getSourcePort(portInfo);
}

std::shared_ptr<IDeviceWatcher> Platform::createGmslDeviceWatcher() {
    auto pal = palMap_.find("gmsl");
    if(pal == palMap_.end()) {
        throw pal_exception("Gmsl pal is not exist, please check the build config that you have enabled BUILD_GMSL_PAL");
    }
    return pal->second->createDeviceWatcher();
}

}  // namespace libobsensor