// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AndroidUsbPal.hpp"
#include "AndroidUsbDeviceManager.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

#include "exception/ObException.hpp"
#include "utils/StringUtils.hpp"

#include <memory>
#include <vector>
#include <string>

namespace libobsensor {

AndroidUsbDeviceManager::AndroidUsbDeviceManager() : gJVM_(nullptr), jObjDeviceWatcher_(nullptr) {
    libUsbCtx_ = std::make_shared<UsbContext>();
}

AndroidUsbDeviceManager::~AndroidUsbDeviceManager() {
    if(gJVM_ == nullptr) {
        LOG_INFO("AndroidUsbDeviceManager Destroyed");
        return;
    }

    JNIEnv *env        = nullptr;
    int     envStatus  = gJVM_->GetEnv((void **)&env, JNI_VERSION_1_6);
    bool    needDetach = false;
    if(envStatus == JNI_EDETACHED) {
        if(gJVM_->AttachCurrentThread(&env, NULL) != 0) {
            LOG_ERROR("JNI error attach current thread");
        }
        needDetach = true;
    }

    std::vector<std::string> devUrlList;
    {
        std::lock_guard<std::recursive_mutex> lk(mutex_);
        for(auto it = deviceHandleMap_.begin(); it != deviceHandleMap_.end(); it++) {
            devUrlList.push_back((*it).first);
        }
    }
    for(const std::string &devUrl: devUrlList) {
        LOG_WARN("~AndroidUsbDeviceManager try closeUsbDevice: %s", devUrl.c_str());
        closeUsbDevice(devUrl);
    }

    if(jObjDeviceWatcher_ != nullptr && env != nullptr) {
        jclass   clsDeviceWatcher  = env->GetObjectClass(jObjDeviceWatcher_);
        jfieldID fieldNativeHandle = env->GetFieldID(clsDeviceWatcher, "mNativeHandle", "J");
        jlong    nativeHandle      = env->GetLongField(jObjDeviceWatcher_, fieldNativeHandle);
        if(reinterpret_cast<long>(this) == nativeHandle) {
            env->SetLongField(jObjDeviceWatcher_, fieldNativeHandle, 0);
        }

        env->DeleteGlobalRef(jObjDeviceWatcher_);
        jObjDeviceWatcher_ = nullptr;
    }

    if(needDetach) {
        gJVM_->DetachCurrentThread();
    }

    gJVM_ = nullptr;
    LOG_DEBUG("AndroidUsbDeviceManager Destroyed");
}

libusb_context *AndroidUsbDeviceManager::getContext() const {
    return libUsbCtx_->getContext();
}

void AndroidUsbDeviceManager::onDeviceChanged(OBDeviceChangedType changedType, const UsbInterfaceInfo &usbDevInfo) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if(changedType == OB_DEVICE_ARRIVAL) {
        LOG_DEBUG("Device Arrival event occurred");
        deviceInfoList_.push_back(usbDevInfo);
    }
    else {
        LOG_DEBUG("Device Removed event occurred");
        auto tarDevIter =
            std::find_if(deviceInfoList_.begin(), deviceInfoList_.end(), [=](const UsbInterfaceInfo &devInfoItem) { return devInfoItem == usbDevInfo; });
        if(tarDevIter != deviceInfoList_.end()) {
            deviceInfoList_.erase(tarDevIter);
        }
    }

    if(callback_) {
        callback_(changedType, usbDevInfo.uid);  // TODO: Change to a URL callback
    }
}

std::vector<UsbInterfaceInfo> AndroidUsbDeviceManager::getDeviceInfoList() {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    return deviceInfoList_;
}

void AndroidUsbDeviceManager::start(deviceChangedCallback callback) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    callback_ = callback;
    // for(const auto &deviceInfo: deviceInfoList_) {
    //     callback_(OB_DEVICE_ARRIVAL, deviceInfo.uid); // TODO: Change to a URL callback
    // }
}

void AndroidUsbDeviceManager::stop() {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    callback_ = nullptr;
}

std::shared_ptr<IUsbDevice> AndroidUsbDeviceManager::openUsbDevice(const std::string &devUrl) {
    LOG_DEBUG("AndroidUsbDeviceManager::openUsbDevice: {}", devUrl);
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    // TODO: Add device list validation
    auto deviceHandleIter = deviceHandleMap_.find(devUrl);
    if(deviceHandleIter != deviceHandleMap_.end()) {
        AndroidUsbDeviceHandle *handle = &deviceHandleIter->second;
        handle->ref++;

        std::shared_ptr<libusb_device_handle> sharedPtrHandle(handle->deviceHandle, [devUrl](libusb_device_handle *handle) {
            (void)handle;
            AndroidUsbDeviceManager::getInstance()->closeUsbDevice(devUrl);
        });
        return std::make_shared<UsbDeviceLibusb>(getContext(), sharedPtrHandle, devUrl);
    }
    else {
        if(!gJVM_) {
            throw libobsensor::pal_exception("AndroidUsbDeviceManager::openUsbDevice gJVM_ is uninitialized!");
        }

        JNIEnv *env;
        int     envStatus  = gJVM_->GetEnv((void **)&env, JNI_VERSION_1_6);
        bool    needDetach = false;
        if(envStatus == JNI_EDETACHED) {
            if(gJVM_->AttachCurrentThread(&env, NULL) != 0) {
                LOG_ERROR("JNI error attach current thread");
                return nullptr;
            }
            needDetach = true;
        }

        jclass    jClsDeviceWatcher = env->GetObjectClass(jObjDeviceWatcher_);
        jmethodID midOpenUsbDevice  = env->GetMethodID(jClsDeviceWatcher, "openUsbDevice", "(Ljava/lang/String;)I");
        if(!midOpenUsbDevice) {
            LOG_ERROR("OpenUsbDevice GetMethodID not found!");
            if(needDetach) {
                gJVM_->DetachCurrentThread();
            }
            return nullptr;
        }

        jstring jDevUrl = env->NewStringUTF(devUrl.c_str());
        jint fileDsc = env->CallIntMethod(jObjDeviceWatcher_, midOpenUsbDevice, jDevUrl);  // TODO: Change to URL
        env->DeleteLocalRef(jDevUrl);
        if(needDetach) {
            gJVM_->DetachCurrentThread();
        }

        AndroidUsbDeviceHandle newHandle;
        newHandle.url = devUrl;
        newHandle.fd  = (intptr_t)fileDsc;
        auto ret      = libusb_wrap_sys_device(getContext(), (intptr_t)fileDsc, &newHandle.deviceHandle);
        if(ret < 0) {
            throw libobsensor::pal_exception("libusb_wrap_sys_device wrap failed!");
        }
        newHandle.device = libusb_get_device(newHandle.deviceHandle);
        libusb_ref_device(newHandle.device);
        libusb_get_device_descriptor(newHandle.device, &newHandle.desc);
        newHandle.ref = 1;
        deviceHandleMap_.insert({ devUrl, newHandle });

        std::shared_ptr<libusb_device_handle> sharedPtrHandle(newHandle.deviceHandle, [devUrl](libusb_device_handle *handle) {
            (void)handle;
            AndroidUsbDeviceManager::getInstance()->closeUsbDevice(devUrl);
        });
        return std::make_shared<UsbDeviceLibusb>(getContext(), sharedPtrHandle, devUrl);
    }
}

void AndroidUsbDeviceManager::closeUsbDevice(const std::string &devUrl) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    auto                                  deviceHandleIter = deviceHandleMap_.find(devUrl);
    if(deviceHandleIter != deviceHandleMap_.end()) {
        deviceHandleIter->second.ref--;
        if(deviceHandleIter->second.ref <= 0) {
            JNIEnv *env;
            int     envStatus  = gJVM_->GetEnv((void **)&env, JNI_VERSION_1_6);
            bool    needDetach = false;
            if(envStatus == JNI_EDETACHED) {
                if(gJVM_->AttachCurrentThread(&env, NULL) != 0) {
                    LOG_ERROR("JNI error attach current thread");
                    return;
                }
                needDetach = true;
            }
            jclass    jClsDeviceWatcher = env->GetObjectClass(jObjDeviceWatcher_);
            jmethodID midCloseUsbDevice = env->GetMethodID(jClsDeviceWatcher, "closeUsbDevice", "(Ljava/lang/String;)V");
            if(!midCloseUsbDevice) {
                LOG_ERROR("CloseUsbDevice GetMethodID not found!");
                if(needDetach) {
                    gJVM_->DetachCurrentThread();
                }
                return;
            }
            libusb_unref_device(deviceHandleIter->second.device);
            libusb_close(deviceHandleIter->second.deviceHandle);

            jstring jDevUrl = env->NewStringUTF(devUrl.c_str());
            env->CallVoidMethod(jObjDeviceWatcher_, midCloseUsbDevice, jDevUrl);  // TODO: Change to URL
            env->DeleteLocalRef(jDevUrl);
            if(needDetach) {
                gJVM_->DetachCurrentThread();
            }
            deviceHandleMap_.erase(deviceHandleIter);
        }
    }
}

void AndroidUsbDeviceManager::registerDeviceWatcher(JNIEnv *env, jclass typeDeviceWatcher, jobject jDeviceWatcher) {
    if(nullptr != jObjDeviceWatcher_) {
        return;
    }

    std::unique_lock<std::mutex> lk(jvmMutex_);
    if(nullptr != jObjDeviceWatcher_) {
        return;
    }

    jfieldID fieldNativeHandle = env->GetFieldID(typeDeviceWatcher, "mNativeHandle", "J");
    env->SetLongField(jDeviceWatcher, fieldNativeHandle, reinterpret_cast<long>(this));

    JavaVM *vm  = nullptr;
    auto    ret = env->GetJavaVM(&vm);
    if(JNI_OK == ret) {
        gJVM_              = vm;
        jObjDeviceWatcher_ = env->NewGlobalRef(jDeviceWatcher);
    }
    else {
        LOG_ERROR("registerDeviceWatcher register JavaVM failed. GetJavaVm failed. ret: %d", ret);
    }
}

void AndroidUsbDeviceManager::addUsbDevice(JNIEnv *env, jobject usbDevInfo) {
    libobsensor::UsbInterfaceInfo usbDeviceInfo;
    jclass                        jcUsbDevInfo = env->GetObjectClass(usbDevInfo);
    jfieldID                      jfUid        = env->GetFieldID(jcUsbDevInfo, "mUid", "I");
    jfieldID                      jfUrl        = env->GetFieldID(jcUsbDevInfo, "mUrl", "Ljava/lang/String;");
    jfieldID                      jfVid        = env->GetFieldID(jcUsbDevInfo, "mVid", "I");
    jfieldID                      jfPid        = env->GetFieldID(jcUsbDevInfo, "mPid", "I");
    jfieldID                      jfMiId       = env->GetFieldID(jcUsbDevInfo, "mMiId", "I");
    jfieldID                      jfSerialNum  = env->GetFieldID(jcUsbDevInfo, "mSerialNum", "Ljava/lang/String;");
    jfieldID                      jfCls        = env->GetFieldID(jcUsbDevInfo, "mCls", "I");
    usbDeviceInfo.uid                          = std::to_string((int)env->GetIntField(usbDevInfo, jfUid));
    jstring jsUrl                              = (jstring)env->GetObjectField(usbDevInfo, jfUrl);
    if (jsUrl != nullptr) {
        const char *szUrl = env->GetStringUTFChars(jsUrl, JNI_FALSE);
        if (szUrl) {
            usbDeviceInfo.url = std::string(szUrl);
            env->ReleaseStringUTFChars(jsUrl, szUrl);
        }
    }
    usbDeviceInfo.vid                          = env->GetIntField(usbDevInfo, jfVid);
    usbDeviceInfo.pid                          = env->GetIntField(usbDevInfo, jfPid);
    usbDeviceInfo.infIndex                     = env->GetIntField(usbDevInfo, jfMiId);
    jstring jsSerialNum                        = (jstring)env->GetObjectField(usbDevInfo, jfSerialNum);
    if(jsSerialNum != nullptr) {
        const char *szSerial = env->GetStringUTFChars(jsSerialNum, JNI_FALSE);
        if(szSerial) {
            usbDeviceInfo.serial = std::string(szSerial);
            env->ReleaseStringUTFChars(jsSerialNum, szSerial);
        }
    }
    usbDeviceInfo.cls    = static_cast<libobsensor::UsbClass>(env->GetIntField(usbDevInfo, jfCls));
    usbDeviceInfo.infUrl = usbDeviceInfo.uid + std::to_string(usbDeviceInfo.infIndex);
    onDeviceChanged(libobsensor::OB_DEVICE_ARRIVAL, usbDeviceInfo);
}

void AndroidUsbDeviceManager::removeUsbDevice(JNIEnv *env, jobject usbDevInfo) {
    libobsensor::UsbInterfaceInfo usbDeviceInfo;
    jclass                        jcUsbDevInfo = env->GetObjectClass(usbDevInfo);
    jfieldID                      jfUid        = env->GetFieldID(jcUsbDevInfo, "mUid", "I");
    jfieldID                      jfUrl        = env->GetFieldID(jcUsbDevInfo, "mUrl", "Ljava/lang/String;");
    jfieldID                      jfVid        = env->GetFieldID(jcUsbDevInfo, "mVid", "I");
    jfieldID                      jfPid        = env->GetFieldID(jcUsbDevInfo, "mPid", "I");
    jfieldID                      jfMiId       = env->GetFieldID(jcUsbDevInfo, "mMiId", "I");
    jfieldID                      jfSerialNum  = env->GetFieldID(jcUsbDevInfo, "mSerialNum", "Ljava/lang/String;");
    jfieldID                      jfCls        = env->GetFieldID(jcUsbDevInfo, "mCls", "I");
    usbDeviceInfo.uid                          = std::to_string((int)env->GetIntField(usbDevInfo, jfUid));
    jstring jsUrl                              = (jstring)env->GetObjectField(usbDevInfo, jfUrl);
    if (jsUrl != nullptr) {
        const char *szUrl = env->GetStringUTFChars(jsUrl, JNI_FALSE);
        if (szUrl) {
            usbDeviceInfo.url = std::string(szUrl);
            env->ReleaseStringUTFChars(jsUrl, szUrl);
        }
    }
    usbDeviceInfo.vid                          = env->GetIntField(usbDevInfo, jfVid);
    usbDeviceInfo.pid                          = env->GetIntField(usbDevInfo, jfPid);
    usbDeviceInfo.infIndex                     = env->GetIntField(usbDevInfo, jfMiId);
    jstring jsSerialNum                        = (jstring)env->GetObjectField(usbDevInfo, jfSerialNum);
    if(jsSerialNum != nullptr) {
        const char *szSerial = env->GetStringUTFChars(jsSerialNum, JNI_FALSE);
        if(szSerial) {
            usbDeviceInfo.serial = std::string(szSerial);
            env->ReleaseStringUTFChars(jsSerialNum, szSerial);
        }
    }
    usbDeviceInfo.cls    = static_cast<libobsensor::UsbClass>(env->GetIntField(usbDevInfo, jfCls));
    usbDeviceInfo.infUrl = usbDeviceInfo.uid + std::to_string(usbDeviceInfo.infIndex);
    onDeviceChanged(libobsensor::OB_DEVICE_REMOVED, usbDeviceInfo);
}

}  // namespace libobsensor

static inline void throw_error(JNIEnv *env, const char *function_name, const char *message) {
    std::string strFunction = (function_name ? std::string(function_name) : "");
    std::string strMessage  = (message ? std::string(message) : "");
    std::string errorMsg    = strFunction + "(), " + strMessage;
    jclass      cls         = env->FindClass("com/orbbec/obsensor/OBException");
    if(nullptr == cls) {
        LOG_ERROR("throw_error failed. not found class: com/orbbec/obsensor/OBException. function_name: %s, errorMsg: %s", function_name, message);
        return;
    }
    env->ThrowNew(cls, errorMsg.c_str());
}

/*
 * Class:     com_orbbec_internal_DeviceWatcher
 * Method:    nRegisterClassObj
 * Signature: (Lcom/orbbec/internal/IDeviceWatcher;)V
 */
extern "C" JNIEXPORT void JNICALL Java_com_orbbec_internal_DeviceWatcher_nRegisterClassObj(JNIEnv *env, jclass typeDeviceWatcher, jobject jDeviceWatcher) {
    libobsensor::AndroidUsbDeviceManager::getInstance()->registerDeviceWatcher(env, typeDeviceWatcher, jDeviceWatcher);
}

/*
 * Class:     com_orbbec_internal_DeviceWatcher
 * Method:    nAddUsbDevice
 * Signature: (Lcom/orbbec/internal/UsbInterfaceInfo;)V
 */
extern "C" JNIEXPORT void JNICALL Java_com_orbbec_internal_DeviceWatcher_nAddUsbDevice(JNIEnv *env, jobject jDeviceWatcher, jobject usbDevInfo) {
    // jclass   clsDeviceWatcher  = env->GetObjectClass(jDeviceWatcher);
    // jfieldID fieldNativeHandle = env->GetFieldID(clsDeviceWatcher, "mNativeHandle", "J");
    // jlong    nativeHandle      = env->GetLongField(jDeviceWatcher, fieldNativeHandle);
    // if(0 == nativeHandle) {
    //     throw_error(env, "nAddUsbDevice", "mNativeHandle=0, invalid pointer");
    // }
    // auto androidUsbManager = reinterpret_cast<libobsensor::AndroidUsbDeviceManager *>(nativeHandle);
    (void)jDeviceWatcher;
    try {
        libobsensor::AndroidUsbDeviceManager::getInstance()->addUsbDevice(env, usbDevInfo);
    }
    catch(libobsensor::libobsensor_exception &e) {
        throw_error(env, "nAddUsbDevice", e.get_message());
    }
}

/*
 * Class:     com_orbbec_internal_DeviceWatcher
 * Method:    nRemoveUsbDevice
 * Signature: (Lcom/orbbec/internal/UsbInterfaceInfo;)V
 */
extern "C" JNIEXPORT void JNICALL Java_com_orbbec_internal_DeviceWatcher_nRemoveUsbDevice(JNIEnv *env, jobject jDeviceWatcher, jobject usbDevInfo) {
    // jclass   clsDeviceWatcher  = env->GetObjectClass(jDeviceWatcher);
    // jfieldID fieldNativeHandle = env->GetFieldID(clsDeviceWatcher, "mNativeHandle", "J");
    // jlong    nativeHandle      = env->GetLongField(jDeviceWatcher, fieldNativeHandle);
    // if(0 == nativeHandle) {
    //     throw_error(env, "nRemovedUsbDevice", "mNativeHandle=0, invalid pointer");
    // }
    // auto androidUsbManager = reinterpret_cast<libobsensor::AndroidUsbDeviceManager *>(nativeHandle);
    (void)jDeviceWatcher;
    try {
        // androidUsbManager->removeUsbDevice(env, usbDevInfo);
        libobsensor::AndroidUsbDeviceManager::getInstance()->removeUsbDevice(env, usbDevInfo);
    }
    catch(libobsensor::libobsensor_exception &e) {
        throw_error(env, "nRemoveUsbDevice", e.get_message());
    }
}
