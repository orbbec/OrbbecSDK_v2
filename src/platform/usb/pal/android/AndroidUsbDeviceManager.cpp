// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AndroidUsbDeviceManager.hpp"

#include <memory>
#include <vector>

#include "core/Context.hpp"
#include "usb/enumerator/DeviceLibusb.hpp"
#include "pal/android/AndroidUsbPal.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utility/StringUtils.hpp"

namespace libobsensor {

AndroidUsbDeviceManager::AndroidUsbDeviceManager() : jObjDeviceWatcher_(nullptr), gJVM_(nullptr) {
    usbCtx_ = std::make_shared<UsbContext>();
}

AndroidUsbDeviceManager::~AndroidUsbDeviceManager() {
    if(gJVM_ == nullptr) {
        LOG_INFO("Destroyed");
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
    LOG_INFO("Destroyed");
}

void AndroidUsbDeviceManager::onDeviceChanged(OBDeviceChangedType changedType, const UsbInterfaceInfo &usbDevInfo) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    LOG_DEBUG("AndroidUsbDeviceManager::notify");
    if(changedType == OB_DEVICE_ARRIVAL) {
        LOG_DEBUG("Device Arrival event occurred");
        deviceInfoList_.push_back(usbDevInfo);
    }
    else {
        LOG_DEBUG("Device Removed event occurred");
        auto tarDevIter = std::find_if(deviceInfoList_.begin(), deviceInfoList_.end(), [=](const UsbInterfaceInfo &devInfoItem) {
            //
            return devInfoItem == usbDevInfo;
        });
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

std::shared_ptr<UsbDevice> AndroidUsbDeviceManager::openUsbDevice(const std::string &devUrl) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    // TODO: Add device list validation
    auto deviceHandleIter = deviceHandleMap_.find(devUrl);
    if(deviceHandleIter != deviceHandleMap_.end()) {
        AndroidUsbDeviceHandle *handle = &deviceHandleIter->second;
        handle->ref++;
        return std::make_shared<UsbDeviceLibusb>(shared_from_this(), handle->device, handle->deviceHandle, handle->desc, usbCtx_, devUrl);
    }
    else {
        if(!gJVM_) {
            throw libobsensor::pal_exception("AndroidUsbDeviceManager::openUsbDevice gJVM_ is uninitialized!", OB_EXCEPTION_TYPE_PLATFORM);
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
        jmethodID midOpenUsbDevice  = env->GetMethodID(jClsDeviceWatcher, "openUsbDevice", "(I)I");
        if(!midOpenUsbDevice) {
            LOG_ERROR("OpenUsbDevice GetMethodID not found!");
            if(needDetach) {
                gJVM_->DetachCurrentThread();
            }
            return nullptr;
        }
        int uid = 0;
        Stringutils::string::cvt2Int(devUrl, uid);
        jint fileDsc = env->CallIntMethod(jObjDeviceWatcher_, midOpenUsbDevice, uid);  // TODO: Change to URL
        if(needDetach) {
            gJVM_->DetachCurrentThread();
        }

        AndroidUsbDeviceHandle newHandle;
        newHandle.url = devUrl;
        newHandle.fd  = (intptr_t)fileDsc;
        auto ret      = libusb_wrap_sys_device(usbCtx_->get(), (intptr_t)fileDsc, &newHandle.deviceHandle);
        if(ret < 0) {
            throw libobsensor::pal_exception("libusb_wrap_sys_device wrap failed!", OB_EXCEPTION_TYPE_IO);
        }
        newHandle.device = libusb_get_device(newHandle.deviceHandle);
        libusb_ref_device(newHandle.device);
        libusb_get_device_descriptor(newHandle.device, &newHandle.desc);
        newHandle.ref = 1;
        deviceHandleMap_.insert({ devUrl, newHandle });
        return std::make_shared<UsbDeviceLibusb>(shared_from_this(), newHandle.device, newHandle.deviceHandle, newHandle.desc, usbCtx_, devUrl);
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
            jmethodID midCloseUsbDevice = env->GetMethodID(jClsDeviceWatcher, "closeUsbDevice", "(I)V");
            if(!midCloseUsbDevice) {
                LOG_ERROR("CloseUsbDevice GetMethodID not found!");
                if(needDetach) {
                    gJVM_->DetachCurrentThread();
                }
                return;
            }
            libusb_unref_device(deviceHandleIter->second.device);
            libusb_close(deviceHandleIter->second.deviceHandle);
            int uid = 0;
            Stringutils::string::cvt2Int(devUrl, uid);
            env->CallVoidMethod(jObjDeviceWatcher_, midCloseUsbDevice, uid);  // TODO: Change to URL
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
    jfieldID                      jfVid        = env->GetFieldID(jcUsbDevInfo, "mVid", "I");
    jfieldID                      jfPid        = env->GetFieldID(jcUsbDevInfo, "mPid", "I");
    jfieldID                      jfMiId       = env->GetFieldID(jcUsbDevInfo, "mMiId", "I");
    jfieldID                      jfSerialNum  = env->GetFieldID(jcUsbDevInfo, "mSerialNum", "Ljava/lang/String;");
    jfieldID                      jfCls        = env->GetFieldID(jcUsbDevInfo, "mCls", "I");
    usbDeviceInfo.uid                          = libobsensor::StringUtils::convert2String((int)env->GetIntField(usbDevInfo, jfUid));
    usbDeviceInfo.url                          = usbDeviceInfo.uid;  // TODO: Use the actual URL
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
    usbDeviceInfo.infUrl = libobsensor::StringUtils::convert2String(usbDeviceInfo.uid) + libobsensor::StringUtils::convert2String(usbDeviceInfo.infIndex);
    onDeviceChanged(libobsensor::OB_DEVICE_ARRIVAL, usbDeviceInfo);
}

void AndroidUsbDeviceManager::removeUsbDevice(JNIEnv *env, jobject usbDevInfo) {
    libobsensor::UsbInterfaceInfo usbDeviceInfo;
    jclass                        jcUsbDevInfo = env->GetObjectClass(usbDevInfo);
    jfieldID                      jfUid        = env->GetFieldID(jcUsbDevInfo, "mUid", "I");
    jfieldID                      jfVid        = env->GetFieldID(jcUsbDevInfo, "mVid", "I");
    jfieldID                      jfPid        = env->GetFieldID(jcUsbDevInfo, "mPid", "I");
    jfieldID                      jfMiId       = env->GetFieldID(jcUsbDevInfo, "mMiId", "I");
    jfieldID                      jfSerialNum  = env->GetFieldID(jcUsbDevInfo, "mSerialNum", "Ljava/lang/String;");
    jfieldID                      jfCls        = env->GetFieldID(jcUsbDevInfo, "mCls", "I");
    usbDeviceInfo.uid                          = libobsensor::StringUtils::convert2String((int)env->GetIntField(usbDevInfo, jfUid));
    usbDeviceInfo.url                          = usbDeviceInfo.uid;  // TODO: Use the real URL
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
    usbDeviceInfo.infUrl = libobsensor::StringUtils::convert2String(usbDeviceInfo.uid) + libobsensor::StringUtils::convert2String(usbDeviceInfo.infIndex);
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
    if(!libobsensor::Context::isInstanceExist()) {
        throw_error(env, "nRegisterClassObj", "Error invalid state. Must create instance of OBContext/ob::Context/ob_context before nRegisterClassObj");
    }
    auto AndroidUsbPal = std::dynamic_pointer_cast<libobsensor::AndroidUsbPal>(libobsensor::Context::getInstance()->getDeviceManager()->getObPal());
    AndroidUsbPal->getAndroidUsbManager()->registerDeviceWatcher(env, typeDeviceWatcher, jDeviceWatcher);
}

/*
 * Class:     com_orbbec_internal_DeviceWatcher
 * Method:    nAddUsbDevice
 * Signature: (Lcom/orbbec/internal/UsbInterfaceInfo;)V
 */
extern "C" JNIEXPORT void JNICALL Java_com_orbbec_internal_DeviceWatcher_nAddUsbDevice(JNIEnv *env, jobject jDeviceWatcher, jobject usbDevInfo) {
    jclass   clsDeviceWatcher  = env->GetObjectClass(jDeviceWatcher);
    jfieldID fieldNativeHandle = env->GetFieldID(clsDeviceWatcher, "mNativeHandle", "J");
    jlong    nativeHandle      = env->GetLongField(jDeviceWatcher, fieldNativeHandle);
    if(0 == nativeHandle) {
        throw_error(env, "nAddUsbDevice", "mNativeHandle=0, invalid pointer");
    }
    auto androidUsbManager = reinterpret_cast<libobsensor::AndroidUsbDeviceManager *>(nativeHandle);

    try {
        androidUsbManager->addUsbDevice(env, usbDevInfo);
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
    jclass   clsDeviceWatcher  = env->GetObjectClass(jDeviceWatcher);
    jfieldID fieldNativeHandle = env->GetFieldID(clsDeviceWatcher, "mNativeHandle", "J");
    jlong    nativeHandle      = env->GetLongField(jDeviceWatcher, fieldNativeHandle);
    if(0 == nativeHandle) {
        throw_error(env, "nRemovedUsbDevice", "mNativeHandle=0, invalid pointer");
    }
    auto androidUsbManager = reinterpret_cast<libobsensor::AndroidUsbDeviceManager *>(nativeHandle);

    try {
        androidUsbManager->removeUsbDevice(env, usbDevInfo);
    }
    catch(libobsensor::libobsensor_exception &e) {
        throw_error(env, "nRemoveUsbDevice", e.get_message());
    }
}

// For API 2.0 TODO ljf++ Further adaptation will be done when API 2.0 is available
// extern "C" JNIEXPORT void JNICALL Java_com_orbbec_obsensor2_DeviceWatcher_nAddUsbDevice(JNIEnv *env, jclass type, jobject usbDevInfo) {
//    if (!libobsensor::Context::isInstanceExist()) {
//        throw_error(env, "nAddUsbDevice", "libobsensor::Context is not instance yet.");
//    }
//    auto obContext = libobsensor::Context::getInstance();
//    auto androidUsbManager = std::dynamic_pointer_cast<libobsensor::AndroidUsbPal>(obContext->getDeviceManager()->getObPal())->getAndroidUsbManager();
//    try {
//        androidUsbManager->addUsbDevice(env, usbDevInfo);
//    } catch (libobsensor::libobsensor_exception &e) {
//        throw_error(env, "nAddUsbDevice", e.get_message());
//    }
//}
//
// extern "C" JNIEXPORT void JNICALL Java_com_orbbec_obsensor2_DeviceWatcher_nRemoveUsbDevice(JNIEnv *env, jclass type, jobject usbDevInfo) {
//    if (!libobsensor::Context::isInstanceExist()) {
//        throw_error(env, "nRemoveUsbDevice", "libobsensor::Context is not instance yet.");
//    }
//    auto obContext = libobsensor::Context::getInstance();
//    auto androidUsbManager = std::dynamic_pointer_cast<libobsensor::AndroidUsbPal>(obContext->getDeviceManager()->getObPal())->getAndroidUsbManager();
//    try {
//        androidUsbManager->removeUsbDevice(env, usbDevInfo);
//    } catch (libobsensor::libobsensor_exception &e) {
//        throw_error(env, "nRemoveUsbDevice", e.get_message());
//    }
//}

