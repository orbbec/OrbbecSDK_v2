// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "WinUsbPal.hpp"

#if(_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <dbt.h>
#include <mfapi.h>
#include <ks.h>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include <cctype>  //Std::tolower
#include <chrono>
#include <memory>
#include <sstream>

#include "usb/uvc/WmfUvcDevicePort.hpp"
#include "usb/hid/HidDevicePort.hpp"
#include "usb/vendor/VendorUsbDevicePort.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

constexpr GUID     GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };
constexpr uint16_t PID_BOOTLOADER_UVC           = 0x0501;
namespace libobsensor {

class WinUsbDeviceWatcher : public IDeviceWatcher {
public:
    WinUsbDeviceWatcher(const IPal *backend);
    ~WinUsbDeviceWatcher() noexcept override;

    void start(deviceChangedCallback callback) override;
    void stop() override;

private:
    void                    run();
    static LRESULT CALLBACK onWinEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool             registerDeviceInterfaceToHwnd(HWND hWnd);

private:
    std::thread eventThread_;
    std::mutex  mutex_;
    struct extra_data {
        const IPal           *backend_;
        deviceChangedCallback callback_;
        bool                  stopped_;
        HWND                  hWnd;
        HDEVNOTIFY            hDevNotifyHW;
        HDEVNOTIFY            hDevNotifyUVC;
        HDEVNOTIFY            hDevNotifySensor;
        HDEVNOTIFY            hDevNotifyHID;
        HDEVNOTIFY            hDevNotifyUSB;
        HDEVNOTIFY            hDevNotifyOpenNI;
    } extraData_;
};

std::vector<std::string> stringSplit(const std::string &string, char separator) {
    std::vector<std::string> tokens;
    std::string::size_type   i1 = 0;
    while(true) {
        auto i2 = string.find(separator, i1);
        if(i2 == std::string::npos) {
            tokens.push_back(string.substr(i1));
            return tokens;
        }
        tokens.push_back(string.substr(i1, i2 - i1));
        i1 = i2 + 1;
    }
}

bool parseSymbolicLink(const std::string &symbolicLink, uint16_t &vid, uint16_t &pid, uint16_t &mi, std::string &uid, std::string &device_guid) {
    auto lowerStr = utils::string::toLower(symbolicLink);
    auto tokens   = stringSplit(lowerStr, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        return false;
    }

    auto ids = stringSplit(tokens[1], '&');
    if(ids[0].size() != 8 || ids[0].substr(0, 4) != "vid_" || !(std::istringstream(ids[0].substr(4, 4)) >> std::hex >> vid)) {
        return false;
    }
    if(ids[1].size() != 8 || ids[1].substr(0, 4) != "pid_" || !(std::istringstream(ids[1].substr(4, 4)) >> std::hex >> pid)) {
        return false;
    }
    if(ids.size() > 2 && (ids[2].size() != 5 || ids[2].substr(0, 3) != "mi_" || !(std::istringstream(ids[2].substr(3, 2)) >> mi))) {
        return false;
    }
    ids = stringSplit(tokens[2], '&');
    if(ids.empty()) {
        return false;
    }

    if(ids.size() > 2)
        uid = ids[1];
    else
        uid = "";

    if(tokens.size() >= 3)
        device_guid = tokens[3];

    return true;
}

std::shared_ptr<IPal> createUsbPal() {
    return std::make_shared<WinUsbPal>();
}

WinUsbPal::WinUsbPal() {
    LOG_DEBUG("WinUsbPal init ...");
    // when using COINIT_APARTMENTTHREADED, calling _pISensor->SetEventSink(NULL) to stop sensor can take several seconds
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

    usbEnumerator_ = IUsbEnumerator::getInstance();
    LOG_DEBUG("WinUsbPal created!");
}

WinUsbPal::~WinUsbPal() noexcept {
    TRY_EXECUTE({
        MFShutdown();
        CoUninitialize();
    });
    LOG_DEBUG("WinUsbPal destroyed!");
}

std::shared_ptr<ISourcePort> WinUsbPal::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
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
        if(pair.first == portInfo) {
            port = pair.second.lock();
            if(port != nullptr) {
                return port;
            }
        }
    }

    switch(portInfo->portType) {
    case SOURCE_PORT_USB_VENDOR: {
        auto usbDev = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<VendorUsbDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }
    case SOURCE_PORT_USB_UVC:
        port = std::make_shared<WmfUvcDevicePort>(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_USB_HID: {
        auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
        auto usbDev      = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            throw libobsensor::camera_disconnected_exception("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<HidDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));

        break;
    }
    default:
        throw libobsensor::invalid_value_exception("unsupported source port type!");
    }

    sourcePortMap_.insert(std::make_pair(portInfo, port));
    return port;
}

std::shared_ptr<IDeviceWatcher> WinUsbPal::createDeviceWatcher() const {
    LOG_DEBUG("Create WinUsbDeviceWatcher!");
    return std::make_shared<WinUsbDeviceWatcher>(this);
}

SourcePortInfoList WinUsbPal::querySourcePortInfos() {
    SourcePortInfoList portInfoList;

    auto action = [&](const UsbInterfaceInfo &info, IMFActivate *) {
        UsbSpec     usbSpec;
        std::string hubId, serial, url;
        if(getUsbDescriptors(info.vid, info.pid, info.uid, hubId, usbSpec, serial, url)) {
            auto portInfo      = std::make_shared<USBSourcePortInfo>();
            portInfo->portType = SOURCE_PORT_USB_UVC;
            portInfo->url      = url;
            portInfo->uid      = info.uid;
            portInfo->vid      = info.vid;
            portInfo->pid      = info.pid;
            portInfo->serial   = serial;
            portInfo->connSpec = usbSpecToString(static_cast<UsbSpec>(usbSpec));
            portInfo->infUrl   = info.infUrl;
            portInfo->infIndex = info.infIndex;
            portInfo->infName  = info.infName;
            portInfo->hubId    = hubId;

            portInfoList.push_back(portInfo);
        }
    };

    WmfUvcDevicePort::foreachUvcDevice(action);

    const auto &usbInfoList = usbEnumerator_->queryUsbInterfaces();
    for(const auto &info: usbInfoList) {
        if(info.vid == 0x2bc5 && (info.cls == OB_USB_CLASS_HID || info.cls == OB_USB_CLASS_VENDOR_SPECIFIC)) {
            // 1. Filter non orbbec devices 2. Filter uvc class
            auto portInfo      = std::make_shared<USBSourcePortInfo>();
            portInfo->portType = info.cls == OB_USB_CLASS_HID ? SOURCE_PORT_USB_HID : SOURCE_PORT_USB_VENDOR;
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

            if(portInfo->infUrl.empty()) {
                LOG_WARN("Found a usb port without interface url! drop it!(device url={} interface index={})", info.url,
                         static_cast<uint32_t>(portInfo->infIndex));
                break;
            }
            portInfoList.push_back(portInfo);
        }
    }
    return portInfoList;
}

WinUsbDeviceWatcher::WinUsbDeviceWatcher(const IPal *backend) {
    extraData_.backend_ = backend;
    extraData_.stopped_ = true;
}

WinUsbDeviceWatcher::~WinUsbDeviceWatcher() noexcept {
    WinUsbDeviceWatcher::stop();
}

void WinUsbDeviceWatcher::start(deviceChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!extraData_.stopped_)
        throw wrong_api_call_sequence_exception("Cannot start a running device_watcher");
    extraData_.stopped_  = false;
    extraData_.callback_ = std::move(callback);
    eventThread_         = std::thread([this]() { run(); });
}

void WinUsbDeviceWatcher::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!extraData_.stopped_) {
        extraData_.stopped_ = true;
        if(eventThread_.joinable())
            eventThread_.join();
    }
}

void WinUsbDeviceWatcher::run() {
    WNDCLASS windowClass      = {};
    LPCWSTR  winClassName     = TEXT("OBMINWINAPP");
    windowClass.lpfnWndProc   = &onWinEvent;
    windowClass.lpszClassName = winClassName;
    UnregisterClass(winClassName, nullptr);

    if(!RegisterClass(&windowClass))
        LOG_WARN("RegisterClass failed.");

    extraData_.hWnd = CreateWindow(winClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, &extraData_);
    if(!extraData_.hWnd)
        throw wrong_api_call_sequence_exception("CreateWindow failed");

    MSG msg;

    while(!extraData_.stopped_) {
        if(PeekMessage(&msg, extraData_.hWnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else  // Yield CPU resources, as this is required for connect/disconnect events only
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // UnregisterDeviceNotification(extraData_.hDevNotifyHW);
    UnregisterDeviceNotification(extraData_.hDevNotifyUVC);
    UnregisterDeviceNotification(extraData_.hDevNotifySensor);
    UnregisterDeviceNotification(extraData_.hDevNotifyHID);
    UnregisterDeviceNotification(extraData_.hDevNotifyUSB);
    UnregisterDeviceNotification(extraData_.hDevNotifyOpenNI);
    DestroyWindow(extraData_.hWnd);
}

std::string wideCharToUTF8(const wchar_t *wStr) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, nullptr, 0, nullptr, nullptr);
    if(size <= 0) {
        return "";
    }
    std::string result(size - 1, '\0');
    if(WideCharToMultiByte(CP_UTF8, 0, wStr, -1, &result[0], size, nullptr, nullptr) != size) {
        return "";
    }
    return result;
}

LRESULT CALLBACK WinUsbDeviceWatcher::onWinEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT lRet = 1;
    switch(message) {
    case WM_CREATE:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams));
        if(!registerDeviceInterfaceToHwnd(hWnd)) {
            auto watcherExtraData      = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            watcherExtraData->stopped_ = true;
        }
        break;

    case WM_QUIT: {
        auto watcherExtraData      = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        watcherExtraData->stopped_ = true;
        break;
    }
    case WM_DEVICECHANGE: {
        if(wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            uint16_t    vid = 0;
            uint16_t    pid;
            uint16_t    mi;
            std::string uid;
            std::string device_guid;
            auto        devIntf      = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(lParam);
            std::string symbolicLink = wideCharToUTF8(devIntf->dbcc_name);
            if(parseSymbolicLink(symbolicLink, vid, pid, mi, uid, device_guid) && vid == 0x2bc5) {
                auto watcherExtraData = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                symbolicLink          = utils::string::toUpper(symbolicLink);
                if(wParam == DBT_DEVICEARRIVAL) {
                    LOG_DEBUG("Device arrival event occurred! symbolicLink={}", symbolicLink);
                    if(devIntf->dbcc_classguid != GUID_DEVINTERFACE_USB_DEVICE || PID_BOOTLOADER_UVC == pid) {
                        watcherExtraData->callback_(OB_DEVICE_ARRIVAL, symbolicLink);
                    }
                }
                else if(wParam == DBT_DEVICEREMOVECOMPLETE) {
                    LOG_DEBUG("Device removed event occurred! symbolicLink={}", symbolicLink);
                    if(devIntf->dbcc_classguid == GUID_DEVINTERFACE_USB_DEVICE) {
                        watcherExtraData->callback_(OB_DEVICE_REMOVED, symbolicLink);
                    }
                }
            }
        }
        break;
    }

    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

bool WinUsbDeviceWatcher::registerDeviceInterfaceToHwnd(HWND hWnd) {
    auto data = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    //===========================register HWmonitor events==============================
    const GUID                    classGuid = { 0x175695cd, 0x30d9, 0x4f87, 0x8b, 0xe3, 0x5a, 0x82, 0x70, 0xf4, 0x9a, 0x31 };
    DEV_BROADCAST_DEVICEINTERFACE devBroadcastDeviceInterface;
    devBroadcastDeviceInterface.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcastDeviceInterface.dbcc_classguid  = classGuid;
    devBroadcastDeviceInterface.dbcc_reserved   = 0;

    data->hDevNotifyHW = RegisterDeviceNotification(hWnd, &devBroadcastDeviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyHW == nullptr) {
        UnregisterDeviceNotification(data->hDevNotifyHW);
        LOG_WARN("Register HW events Failed!");
        return false;
    }

    //===========================register UVC events==============================
    DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
    di.dbcc_size                     = sizeof(di);
    di.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    di.dbcc_classguid                = KSCATEGORY_CAPTURE;

    data->hDevNotifyUVC = RegisterDeviceNotification(hWnd, &di, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyUVC == nullptr) {
        UnregisterDeviceNotification(data->hDevNotifyUVC);
        LOG_WARN("Register UVC events Failed!");
        return false;
    }

    //===========================register UVC sensor camera events==============================
    DEV_BROADCAST_DEVICEINTERFACE di_sensor = { 0 };
    di_sensor.dbcc_size                     = sizeof(di_sensor);
    di_sensor.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    di_sensor.dbcc_classguid                = KSCATEGORY_SENSOR_CAMERA;

    data->hDevNotifySensor = RegisterDeviceNotification(hWnd, &di_sensor, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifySensor == nullptr) {
        UnregisterDeviceNotification(data->hDevNotifySensor);
        LOG_WARN("Register UVC events Failed!");
        return false;
    }

    //===========================register HID sensor camera events==============================
    static constexpr GUID GUID_DEVINTERFACE_HID = { 0x4d1e55b2, 0xf16f, 0x11cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

    DEV_BROADCAST_DEVICEINTERFACE hid_sensor = { 0 };
    hid_sensor.dbcc_size                     = sizeof(hid_sensor);
    hid_sensor.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    hid_sensor.dbcc_classguid                = GUID_DEVINTERFACE_HID;

    data->hDevNotifyHID = RegisterDeviceNotification(hWnd, &hid_sensor, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyHID == nullptr) {
        UnregisterDeviceNotification(data->hDevNotifyHID);
        LOG_WARN("Register hid events Failed!");
        return false;
    }

    //===========================register usb device events==============================
    DEV_BROADCAST_DEVICEINTERFACE devBroadcastUsbDeviceInterface;
    devBroadcastUsbDeviceInterface.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devBroadcastUsbDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcastUsbDeviceInterface.dbcc_classguid  = GUID_DEVINTERFACE_USB_DEVICE;
    devBroadcastUsbDeviceInterface.dbcc_reserved   = 0;

    data->hDevNotifyUSB = RegisterDeviceNotification(hWnd, &devBroadcastUsbDeviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyUSB == nullptr) {
        UnregisterDeviceNotification(data->hDevNotifyUSB);
        LOG_WARN("Register HW events Failed!");
        return false;
    }

    return true;
}

}  // namespace libobsensor

