// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"
#include "usb/enumerator/UsbContext.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

#include <string>
#include <map>
#include <memory>
#include <jni.h>

namespace libobsensor {
struct AndroidUsbDeviceHandle {
    std::string              url = "";
    intptr_t                 fd  = NULL;
    libusb_device_descriptor desc;
    libusb_device           *device       = nullptr;
    libusb_device_handle    *deviceHandle = nullptr;

    uint16_t ref = 0;
};

class AndroidUsbDeviceManager : public IDeviceWatcher, public std::enable_shared_from_this<AndroidUsbDeviceManager> {
public:
    static std::shared_ptr<AndroidUsbDeviceManager> getInstance() {
        static std::shared_ptr<AndroidUsbDeviceManager> instance(new AndroidUsbDeviceManager());
        return instance;
    }

    virtual ~AndroidUsbDeviceManager() noexcept override;

    virtual void                  start(deviceChangedCallback callback) override;
    virtual void                  stop() override;
    void                          onDeviceChanged(OBDeviceChangedType changeType_, const UsbInterfaceInfo &usbDevInfo);
    std::vector<UsbInterfaceInfo> getDeviceInfoList();

    std::shared_ptr<IUsbDevice> openUsbDevice(const std::string &devUrl);
    void                        closeUsbDevice(const std::string &devUrl);

    void addUsbDevice(JNIEnv *env, jobject usbDevInfo);

    void removeUsbDevice(JNIEnv *env, jobject usbDevInfo);

    void registerDeviceWatcher(JNIEnv *env, jclass typeDeviceWatcher, jobject jDeviceWatcher);

    libusb_context *getContext() const;

private:
    AndroidUsbDeviceManager();

    AndroidUsbDeviceManager(const AndroidUsbDeviceManager &)            = delete;
    AndroidUsbDeviceManager &operator=(const AndroidUsbDeviceManager &) = delete;

private:
    std::shared_ptr<UsbContext>                   libUsbCtx_;
    std::recursive_mutex                          mutex_;
    deviceChangedCallback                         callback_ = nullptr;
    std::vector<UsbInterfaceInfo>                 deviceInfoList_;
    std::map<std::string, AndroidUsbDeviceHandle> deviceHandleMap_;

    std::mutex       jvmMutex_;
    JavaVM          *gJVM_;
    volatile jobject jObjDeviceWatcher_;
};

}  // namespace libobsensor
