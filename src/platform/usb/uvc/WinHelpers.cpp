// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if(_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#include "WinHelpers.hpp"

#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include <Cfgmgr32.h>
#include <Sddl.h>
#include <SetupAPI.h>
#include <Windows.h>
#include <atlstr.h>
#include <comdef.h>
#include <regex>
#include <string>
#include <algorithm>
#include <usbioctl.h>
#include <winioctl.h>

#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "setupapi.lib")

#include <initguid.h>

// https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/supported-usb-classes#microsoft-provided-usb-device-class-drivers

DEFINE_GUID(GUID_DEVINTERFACE_IMAGE_WIN10, 0x6bdd1fc6L, 0x810f, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f);
DEFINE_GUID(GUID_DEVINTERFACE_CAMERA_WIN10, 0xca3e7ab9, 0xb4c3, 0x4ae6, 0x82, 0x51, 0x57, 0x9e, 0xf9, 0x33, 0x89, 0x0f);

#define CREATE_MUTEX_RETRY_NUM (5)

namespace libobsensor {

template <typename T> size_t vector_bytes_size(const typename std::vector<T> &vec) {
    static_assert((std::is_arithmetic<T>::value), "vector_bytes_size requires numeric type for input data");
    return sizeof(T) * vec.size();
}

std::string wcharToString(const wchar_t *wText) {
    DWORD dwNum = WideCharToMultiByte(CP_UTF8, NULL, wText, -1, nullptr, 0, nullptr, FALSE);  // Application of WideCharToMultiByte
    char *psText;  // psText is a temporary char* array used as an intermediate variable for assignment to std::string
    psText = new char[dwNum];
    WideCharToMultiByte(CP_UTF8, NULL, wText, -1, psText, dwNum, NULL, FALSE);  // Reuse of WideCharToMultiByte
    std::string szDst = psText;                                                 // std::string assignment
    delete[] psText;                                                            // Clearing psText
    return szDst;
}

std::string hr_to_string(HRESULT hr) {
    _com_error        err(hr);
    std::wstring      errorMessage = (err.ErrorMessage()) ? err.ErrorMessage() : L"";
    std::stringstream ss;
    ss << "HResult 0x" << std::hex << hr << ": \"" << wcharToString(errorMessage.c_str()) << "\"";
    return ss.str();
}

typedef ULONG(__stdcall *fnRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);

bool is_win10_redstone2() {
    RTL_OSVERSIONINFOEXW verInfo = { 0 };
    verInfo.dwOSVersionInfoSize  = sizeof(verInfo);
    static auto RtlGetVersion    = reinterpret_cast<fnRtlGetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if(RtlGetVersion != nullptr && RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&verInfo)) == 0) {
        return verInfo.dwMajorVersion >= 0x0A && verInfo.dwBuildNumber >= 15063;
    }
    else
        return false;
}

bool check(const char *call, HRESULT hr, bool to_throw) {
    if(SUCCEEDED(hr)) {
        return true;
    }

    std::string descr = utils::string::to_string() << call << " returned: " << hr_to_string(hr);
    if(to_throw) {
        LOG_ERROR(descr);
        throw pal_exception(descr);
    }
    LOG_DEBUG(descr);
    return false;
}

std::string win_to_utf(const WCHAR *s) {
    auto len = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if(len == 0)
        throw std::runtime_error(utils::string::to_string() << "WideCharToMultiByte(...) returned 0 and GetLastError() is " << GetLastError());

    std::string buffer(len - 1, ' ');
    len = WideCharToMultiByte(CP_UTF8, 0, s, -1, &buffer[0], static_cast<int>(buffer.size()) + 1, nullptr, nullptr);
    if(len == 0)
        throw std::runtime_error(utils::string::to_string() << "WideCharToMultiByte(...) returned 0 and GetLastError() is " << GetLastError());

    return buffer;
}

std::vector<std::string> tokenize(std::string string, char separator) {
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

// Parse the following USB path format = \?usb#vid_vvvv&pid_pppp&mi_ii#aaaaaaaaaaaaaaaa#{gggggggg-gggg-gggg-gggg-gggggggggggg}
// vvvv = USB vendor ID represented in 4 hexadecimal characters.
// pppp = USB product ID represented in 4 hexadecimal characters.
// ii = USB interface number.
// aaaaaaaaaaaaaaaa = unique Windows-generated string based on things such as the physical USB port address and/or interface number.
// gggggggg-gggg-gggg-gggg-gggggggggggg = device interface GUID assigned in the driver or driver INF file and is used to link applications to device with
// specific drivers loaded.
bool parse_usb_path_multiple_interface(uint16_t &vid, uint16_t &pid, uint16_t &mi, std::string &unique_id, const std::string &path, std::string &device_guid) {
    auto name   = utils::string::toLower(path);
    auto tokens = tokenize(name, '#');
    if(tokens.size() < 1 || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return false;
    }

    auto ids = tokenize(tokens[1], '&');
    if(ids[0].size() != 8 || ids[0].substr(0, 4) != "vid_" || !(std::istringstream(ids[0].substr(4, 4)) >> std::hex >> vid)) {
        LOG_ERROR("malformed vid string: {}", tokens[1]);
        return false;
    }

    if(ids[1].size() != 8 || ids[1].substr(0, 4) != "pid_" || !(std::istringstream(ids[1].substr(4, 4)) >> std::hex >> pid)) {
        LOG_ERROR("malformed pid string: {}", tokens[1]);
        return false;
    }

    if(ids.size() > 2 && (ids[2].size() != 5 || ids[2].substr(0, 3) != "mi_" || !(std::istringstream(ids[2].substr(3, 2)) >> mi))) {
        LOG_ERROR("malformed mi string {}", tokens[1]);
        return false;
    }

    ids = tokenize(tokens[2], '&');
    if(ids.size() == 0) {
        LOG_ERROR("malformed id string: {}", tokens[2]);
        return false;
    }

    if(ids.size() > 2)
        unique_id = ids[1];
    else
        unique_id = "";

    if(tokens.size() >= 3)
        device_guid = tokens[3];

    return true;
}

// Parse the following USB path format = \?usb#vid_vvvv&pid_pppp#ssss#{gggggggg-gggg-gggg-gggg-gggggggggggg}
// vvvv = USB vendor ID represented in 4 hexadecimal characters.
// pppp = USB product ID represented in 4 hexadecimal characters.
// ssss = USB serial string represented in n characters.
// gggggggg-gggg-gggg-gggg-gggggggggggg = device interface GUID assigned in the driver or driver INF file and is used to link applications to device with
// specific drivers loaded.
bool parse_usb_path_single_interface(uint16_t &vid, uint16_t &pid, std::string &serial, const std::string &path) {
    auto name   = utils::string::toLower(path);
    auto tokens = tokenize(name, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return false;
    }

    auto ids = tokenize(tokens[1], '&');
    if(ids[0].size() != 8 || ids[0].substr(0, 4) != "vid_" || !(std::istringstream(ids[0].substr(4, 4)) >> std::hex >> vid)) {
        LOG_ERROR("malformed vid string: {}", tokens[1]);
        return false;
    }

    if(ids[1].size() != 8 || ids[1].substr(0, 4) != "pid_" || !(std::istringstream(ids[1].substr(4, 4)) >> std::hex >> pid)) {
        LOG_ERROR("malformed pid string: {}", tokens[1]);
        return false;
    }

    serial = tokens[2];

    return true;
}

bool parse_usb_path_from_device_id(uint16_t &vid, uint16_t &pid, uint16_t &mi, std::string &unique_id, const std::string &device_id) {
    auto name   = utils::string::toLower(device_id);
    auto tokens = tokenize(name, '\\');
    if(tokens.size() < 1 || tokens[0] != R"(usb)")
        return false;  // Not a USB device

    auto ids = tokenize(tokens[1], '&');

    if(ids.size() < 3) {
        LOG_ERROR("incomplete device id");
        return false;
    }

    if(ids[0].size() != 8 || ids[0].substr(0, 4) != "vid_" || !(std::istringstream(ids[0].substr(4, 4)) >> std::hex >> vid)) {
        LOG_ERROR("malformed vid string: {}", tokens[1]);
        return false;
    }

    if(ids[1].size() != 8 || ids[1].substr(0, 4) != "pid_" || !(std::istringstream(ids[1].substr(4, 4)) >> std::hex >> pid)) {
        LOG_ERROR("malformed pid string: {}", tokens[1]);
        return false;
    }

    if(ids[2].size() != 5 || ids[2].substr(0, 3) != "mi_" || !(std::istringstream(ids[2].substr(3, 2)) >> mi)) {
        LOG_ERROR("malformed mi string: {}", tokens[1]);
        return false;
    }

    ids = tokenize(tokens[2], '&');
    if(ids.size() < 2) {
        LOG_ERROR("malformed id string: {}", tokens[2]);
        return false;
    }
    unique_id = ids[1];
    return true;
}

/*
UvcInfo.location example :

"   \\\\.\\USB#VID_05E3&PID_0608#5&1e7d8db7&0&5#{f18a0e88-c30c-11d0-8815-00a0c906bed8} 2"
"\\\\.\\USB#VID_05E3&PID_0608#5&1e7d8db7&0&5#{f18a0e88-c30c-11d0-8815-00a0c906bed8} 1"
"\\\\.\\USB#VID_05E3&PID_0608#5&1e7d8db7&0&5#{f18a0e88-c30c-11d0-8815-00a0c906bed8} 1"

"\\\\.\\USB#VID_05E3&PID_0608#5&1e7d8db7&0&1#{f18a0e88-c30c-11d0-8815-00a0c906bed8} 2"
"\\\\.\\USB#VID_05E3&PID_0608#5&1e7d8db7&0&1#{f18a0e88-c30c-11d0-8815-00a0c906bed8} 1"
"\\\\.\\USB # VID_05E3&PID_0608 # 5&1e7d8db7&0&1 # {f18a0e88-c30c-11d0-8815-00a0c906bed8} 1"

*/

bool parse_hubid_from_location(const std::string location, std::string &hubuid) {
    auto name   = utils::string::toLower(location);
    auto tokens = tokenize(name, '#');
    if(tokens.empty())
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        LOG_ERROR("malformed usb device path: {}", name);
        return false;
    }
    hubuid = tokens[2];

    return true;
}

bool handle_node(const std::wstring &targetKey, HANDLE h, ULONG index) {
    USB_NODE_CONNECTION_DRIVERKEY_NAME key;
    key.ConnectionIndex = index;

    if(!DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, &key, sizeof(key), &key, sizeof(key), nullptr, nullptr)) {
        return false;
    }

    if(key.ActualLength < sizeof(key))
        return false;

    auto alloc = std::malloc(key.ActualLength);
    if(!alloc)
        throw std::bad_alloc();

    auto pKey = std::shared_ptr<USB_NODE_CONNECTION_DRIVERKEY_NAME>(reinterpret_cast<USB_NODE_CONNECTION_DRIVERKEY_NAME *>(alloc), std::free);

    pKey->ConnectionIndex = index;
    if(DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, pKey.get(), key.ActualLength, pKey.get(), key.ActualLength, nullptr, nullptr)) {
        // std::wcout << pKey->DriverKeyName << std::endl;
        if(targetKey == pKey->DriverKeyName) {
            return true;
        }
        else
            return false;
    }

    return false;
}

std::wstring get_path(HANDLE h, ULONG index) {
    // get name length
    USB_NODE_CONNECTION_NAME name;
    name.ConnectionIndex = index;
    if(!DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_NAME, &name, sizeof(name), &name, sizeof(name), nullptr, nullptr)) {
        return std::wstring(L"");
    }

    // alloc space
    if(name.ActualLength < sizeof(name))
        return std::wstring(L"");
    auto alloc = std::malloc(name.ActualLength);
    auto pName = std::shared_ptr<USB_NODE_CONNECTION_NAME>(reinterpret_cast<USB_NODE_CONNECTION_NAME *>(alloc), std::free);

    // get name
    pName->ConnectionIndex = index;
    if(DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_NAME, pName.get(), name.ActualLength, pName.get(), name.ActualLength, nullptr, nullptr)) {
        return std::wstring(pName->NodeName);
    }

    return std::wstring(L"");
}

std::tuple<std::string, UsbSpec> handle_usb_hub(const std::wstring &targetKey, const std::wstring &path) {
    auto res = std::make_tuple(std::string(""), UsbSpec::usb_undefined);

    if(path.empty())
        return res;
    std::wstring fullPath = L"\\\\.\\" + path;

    HANDLE h = CreateFile(fullPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if(h == INVALID_HANDLE_VALUE)
        return res;
    auto h_gc = std::shared_ptr<void>(h, CloseHandle);

    USB_NODE_INFORMATION info{};
    if(!DeviceIoControl(h, IOCTL_USB_GET_NODE_INFORMATION, &info, sizeof(info), &info, sizeof(info), nullptr, nullptr))
        return res;

    // for each port on the hub
    for(ULONG i = 1; i <= info.u.HubInformation.HubDescriptor.bNumberOfPorts; ++i) {
        // allocate something or other
        char                                buf[sizeof(USB_NODE_CONNECTION_INFORMATION_EX)] = { 0 };
        PUSB_NODE_CONNECTION_INFORMATION_EX pConInfo                                        = reinterpret_cast<PUSB_NODE_CONNECTION_INFORMATION_EX>(buf);

        // get info about port i
        pConInfo->ConnectionIndex = i;
        if(!DeviceIoControl(h, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, pConInfo, sizeof(buf), pConInfo, sizeof(buf), nullptr, nullptr)) {
            continue;
        }

        // check if device is connected
        if(pConInfo->ConnectionStatus != DeviceConnected) {
            continue;  // almost assuredly silently. I think this flag gets set for any port without a device
        }

        // if connected, handle correctly, setting the location info if the device is found
        if(pConInfo->DeviceIsHub)
            res = handle_usb_hub(targetKey, get_path(h, i));  // Invoke recursion to traverse USB hubs chain
        else {
            if(handle_node(targetKey, h, i))  // exit condition
            {
                return std::make_tuple(win_to_utf(fullPath.c_str()) + " " + std::to_string(i), static_cast<UsbSpec>(pConInfo->DeviceDescriptor.bcdUSB));
            }
        }

        if(std::string("") != std::get<0>(res))
            return res;
    }

    return res;
}

// Provides Port Id and the USB Specification (USB type)
bool getUsbDescriptors(uint16_t device_vid, uint16_t device_pid, const std::string &device_uid, std::string &hubuid, UsbSpec &spec, std::string &serial,
                       std::string &url) {
    SP_DEVINFO_DATA   devInfo = { sizeof(SP_DEVINFO_DATA) };
    std::vector<GUID> guids   = { GUID_DEVINTERFACE_IMAGE_WIN10, GUID_DEVINTERFACE_CAMERA_WIN10 };

    for(auto guid: guids) {
        // Build a device info represent all imaging devices
        HDEVINFO device_info = SetupDiGetClassDevsEx(static_cast<const GUID *>(&guid), nullptr, nullptr, DIGCF_PRESENT, nullptr, nullptr, nullptr);

        // Add automatic destructor to the device info
        auto di = std::shared_ptr<void>(device_info, SetupDiDestroyDeviceInfoList);

        if(device_info == INVALID_HANDLE_VALUE) {
            return false;
        }

        // Enumerate all imaging devices
        for(int member_index = 0;; ++member_index) {
            SP_DEVICE_INTERFACE_DATA interfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
            unsigned long            buf_size      = 0;

            // Get device information element from the device information set
            if(SetupDiEnumDeviceInfo(device_info, member_index, &devInfo) == FALSE) {
                if(GetLastError() == ERROR_NO_MORE_ITEMS)
                    break;  // stop when none left
                continue;   // silently ignore other errors
            }

            // Get the buffer size required to hold this device instance ID
            if(CM_Get_Device_ID_Size(&buf_size, devInfo.DevInst, 0) != CR_SUCCESS) {
                LOG_ERROR("CM_Get_Device_ID_Size failed");
                return false;
            }

            std::vector<WCHAR> pInstID(buf_size + 1);

            // Get the device ID of current device
            if(CM_Get_Device_ID(devInfo.DevInst, pInstID.data(), (ULONG)vector_bytes_size(pInstID), 0) != CR_SUCCESS) {
                LOG_ERROR("CM_Get_Device_ID failed");
                return false;
            }

            // Check if this is our device
            uint16_t    usb_vid, usb_pid, usb_mi;
            std::string usb_unique_id;
            if(!parse_usb_path_from_device_id(usb_vid, usb_pid, usb_mi, usb_unique_id, std::string(win_to_utf(pInstID.data()))))
                continue;
            if(usb_vid != device_vid || usb_pid != device_pid || /* usb_mi != device->mi || */ usb_unique_id != device_uid)
                continue;

            // Get parent (composite device) instance
            DEVINST instance;
            if(CM_Get_Parent(&instance, devInfo.DevInst, 0) != CR_SUCCESS) {
                LOG_ERROR("CM_Get_Parent failed");
                return false;
            }

            // Get the buffer size required to hold the parent (composite) device instance ID
            if(CM_Get_Device_ID_Size(&buf_size, instance, 0) != CR_SUCCESS) {
                LOG_ERROR("CM_Get_Device_ID_Size failed");
                return false;
            }

            std::vector<WCHAR> pInstID2(buf_size + 1);

            if(CM_Get_Device_ID(instance, pInstID2.data(), ULONG(vector_bytes_size(pInstID2)), 0) != CR_SUCCESS) {
                LOG_ERROR("CM_Get_Device_ID failed");
                return false;
            }

            // Upgrade to DEVINFO_DATA for SetupDiGetDeviceRegistryProperty
            device_info = SetupDiGetClassDevs(nullptr, pInstID2.data(), nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE | DIGCF_ALLCLASSES);

            // Add automatic destructor to the device info
            di = std::shared_ptr<void>(device_info, SetupDiDestroyDeviceInfoList);

            if(device_info == INVALID_HANDLE_VALUE) {
                LOG_ERROR("SetupDiGetClassDevs failed");
                return false;
            }

            interfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
            if(SetupDiEnumDeviceInterfaces(device_info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, 0, &interfaceData) == FALSE) {
                LOG_ERROR("SetupDiEnumDeviceInterfaces failed");
                return false;
            }

            // get the SP_DEVICE_INTERFACE_DETAIL_DATA object, and also grab the SP_DEVINFO_DATA object for the device
            buf_size = 0;
            SetupDiGetDeviceInterfaceDetail(device_info, &interfaceData, nullptr, 0, &buf_size, nullptr);
            if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                LOG_ERROR("SetupDiGetDeviceInterfaceDetail failed");
                return false;
            }

            std::vector<BYTE>                detail_data_buff(buf_size);
            SP_DEVICE_INTERFACE_DETAIL_DATA *detail_data = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA *>(detail_data_buff.data());

            detail_data->cbSize         = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            SP_DEVINFO_DATA parent_data = { sizeof(SP_DEVINFO_DATA) };
            if(!SetupDiGetDeviceInterfaceDetail(device_info, &interfaceData, detail_data, ULONG(vector_bytes_size(detail_data_buff)), nullptr, &parent_data)) {
                LOG_ERROR("SetupDiGetDeviceInterfaceDetail failed");
                return false;
            }

            uint16_t    vid = 0;
            uint16_t    pid = 0;
            uint16_t    mi  = 0;
            std::string uid, devGuid;
            std::string path = win_to_utf(detail_data->DevicePath);
            url              = utils::string::toUpper(path);

            /* Parse the following USB path format = \?usb#vid_vvvv&pid_pppp&mi_ii#aaaaaaaaaaaaaaaa#{gggggggg-gggg-gggg-gggg-gggggggggggg} */
            parse_usb_path_multiple_interface(vid, pid, mi, uid, path, devGuid);
            if(uid.empty()) {
                /* Parse the following USB path format = \?usb#vid_vvvv&pid_pppp#ssss#{gggggggg - gggg - gggg - gggg - gggggggggggg} */
                parse_usb_path_single_interface(vid, pid, serial, path);
                serial = utils::string::toUpper(serial);
            }

            // get driver key for composite device
            buf_size = 0;
            SetupDiGetDeviceRegistryProperty(device_info, &parent_data, SPDRP_DRIVER, nullptr, nullptr, 0, &buf_size);
            if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                LOG_ERROR("SetupDiGetDeviceRegistryProperty failed in an unexpected manner");
                return false;
            }

            std::vector<BYTE> driver_key(buf_size);

            if(!SetupDiGetDeviceRegistryProperty(device_info, &parent_data, SPDRP_DRIVER, nullptr, driver_key.data(), (DWORD)vector_bytes_size(driver_key),
                                                 nullptr)) {
                LOG_ERROR("SetupDiGetDeviceRegistryProperty failed");
                return false;
            }

            // contains composite device key
            std::wstring targetKey(reinterpret_cast<const wchar_t *>(driver_key.data()));

            // recursively check all hubs, searching for composite device
            for(int i = 0;; i++) {
                std::wstringstream buf;
                buf << "\\\\.\\HCD" << i;
                std::wstring hcd = buf.str();

                // grab handle
                HANDLE h    = CreateFile(hcd.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                auto   h_gc = std::shared_ptr<void>(h, CloseHandle);
                if(h == INVALID_HANDLE_VALUE) {
                    LOG_ERROR("CreateFile failed");
                    break;
                }
                else {
                    USB_ROOT_HUB_NAME name;

                    // get required space
                    if(!DeviceIoControl(h, IOCTL_USB_GET_ROOT_HUB_NAME, nullptr, 0, &name, sizeof(name), nullptr, nullptr)) {
                        LOG_ERROR("DeviceIoControl failed");
                        return false;  // alt: fail silently and hope its on a different root hub
                    }

                    std::vector<char>  name_buff(name.ActualLength);
                    USB_ROOT_HUB_NAME *pName = reinterpret_cast<USB_ROOT_HUB_NAME *>(name_buff.data());

                    // get name
                    if(!DeviceIoControl(h, IOCTL_USB_GET_ROOT_HUB_NAME, nullptr, 0, pName, (DWORD)vector_bytes_size(name_buff), nullptr, nullptr)) {
                        LOG_ERROR("DeviceIoControl failed");
                        return false;  // alt: fail silently and hope its on a different root hub
                    }

                    // return location if device is connected under this root hub, also provide the port USB spec/speed
                    auto usb_res = handle_usb_hub(targetKey, std::wstring(pName->RootHubName));
                    if(std::get<0>(usb_res) != "") {

                        parse_hubid_from_location(std::get<0>(usb_res), hubuid);
                        // LOG_ERROR("uvc hubid = " << hubuid;
                        spec = std::get<1>(usb_res);
                        return true;
                    }
                }
            }
        }
    }

    LOG_ERROR("could not find camera in windows device tree");
    return false;
}

bool get_id(DEVINST devinst, std::string *p_out_str) {
    ULONG cch_required = 0;
    if(CM_Get_Device_ID_Size(&cch_required, devinst, 0) != CR_SUCCESS)
        return false;

    if(p_out_str) {
        std::vector<WCHAR> buf(cch_required + 1);
        if(CM_Get_Device_ID(devinst, buf.data(), cch_required, 0) != CR_SUCCESS)
            return false;
        *p_out_str = win_to_utf(buf.data());
    }

    return true;
}

std::string getId(DEVINST devinst) {
    std::string id;
    get_id(devinst, &id);
    return id;
}

std::string cm_node::get_id() const {
    return getId(get());
}

std::string cm_node::get_uid() const {
    uint16_t    vid, pid, mi;
    std::string uid;
    if(!parse_usb_path_from_device_id(vid, pid, mi, uid, get_id()))
        return std::string();
    return uid;
}

/*
    Convert a device path:
        \\?\HID#VID_8086&PID_0B4D&MI_05#7&24fd3503&0&0000#{c317c286-c468-4288-9975-d4c4587c442c}\{560421A4-2F8D-47A0-A6D8-4110C6B2A202}
    to an instance ID:
        HID\VID_8086&PID_0B4D&MI_05\7&217e81dc&0&0000
    which can then be used to get a DEVINST from the config manager.

    Returns an empty string on failure.
*/
std::wstring instance_id_from_device_path(LPCWSTR path) {
    if(wcsncmp(path, L"\\\\?\\", 4))
        return std::wstring();
    std::wstring inst_id(path + 4);
    // Remove the last "#{...}" part, and replace all '#' with '\' on the way:
    for(auto x = inst_id.find(L'#'); x != std::wstring::npos; x = inst_id.find(L'#', x + 1)) {
        if(inst_id[x + 1] == L'{') {
            inst_id.resize(x);
            break;
        }
        inst_id.replace(x, 1, 1, L'\\');
    }
    return inst_id;
}

/* static */ cm_node cm_node::root() {
    DEVINST devinst;
    if(CM_Locate_DevNode(&devinst, nullptr, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
        return cm_node();
    return cm_node(devinst);
}

/* static */ cm_node cm_node::from_instance_id(std::wstring const &inst_id) {
    DEVINST devinst;
    if(CM_Locate_DevNode(&devinst, const_cast<DEVINSTID>(inst_id.data()), CM_LOCATE_DEVNODE_PHANTOM) != CR_SUCCESS)
        return cm_node();
    return cm_node(devinst);
}

cm_node cm_node::from_device_path(LPCWSTR device_path) {
    std::wstring instance_id = instance_id_from_device_path(device_path);
    return from_instance_id(instance_id);
}

cm_node cm_node::get_parent() const {
    DEVINST parent;
    if(CM_Get_Parent(&parent, get(), 0) != CR_SUCCESS)
        return cm_node();
    return cm_node(parent);
}

cm_node cm_node::get_child() const {
    DEVINST child;
    if(CM_Get_Child(&child, get(), 0) != CR_SUCCESS)
        return cm_node();
    return cm_node(child);
}

cm_node cm_node::get_sibling() const {
    DEVINST sibling;
    if(CM_Get_Sibling(&sibling, get(), 0) != CR_SUCCESS)
        return cm_node();
    return cm_node(sibling);
}

std::string cm_node::get_property(DEVPROPKEY const &property) const {
    DEVPROPTYPE type;
    ULONG       cb = 0;
    auto        rv = CM_Get_DevNode_Property(get(), &property, &type, nullptr, &cb, 0);
    if(rv != CR_BUFFER_SMALL)
        return std::string();
    if(type != DEVPROP_TYPE_STRING)
        return std::string();
    std::wstring str;
    str.reserve(cb);
    if(CM_Get_DevNode_Property(get(), &property, &type, (PBYTE)str.data(), &cb, 0) != CR_SUCCESS)
        return std::string();
    return win_to_utf(str.data());
}

#define MAX_HANDLES 64

event_base::event_base(HANDLE handle) : _handle(handle) {}

event_base::~event_base() noexcept {
    if(_handle != nullptr) {
        CloseHandle(_handle);
        _handle = nullptr;
    }
}

bool event_base::set() {
    if(_handle == nullptr)
        return false;
    SetEvent(_handle);
    return true;
}

bool event_base::wait(DWORD timeout) const {
    if(_handle == nullptr)
        return false;

    return WaitForSingleObject(_handle, timeout) == WAIT_OBJECT_0;  // Return true only if object was signaled
}

event_base *event_base::wait(const std::vector<event_base *> &events, bool waitAll, int timeout) {
    if(events.size() > MAX_HANDLES)
        return nullptr;  // WaitForMultipleObjects doesn't support waiting on more then 64 handles

    HANDLE handles[MAX_HANDLES];
    auto   i = 0;
    for(auto &event: events) {
        handles[i] = event->get_handle();
        ++i;
    }
    auto res = WaitForMultipleObjects(static_cast<DWORD>(events.size()), handles, waitAll, timeout);
    if(res < (WAIT_OBJECT_0 + events.size())) {
        return events[res - WAIT_OBJECT_0];
    }
    else {
        return nullptr;
    }
}

event_base *event_base::wait_all(const std::vector<event_base *> &events, int timeout) {
    return wait(events, true, timeout);
}

event_base *event_base::wait_any(const std::vector<event_base *> &events, int timeout) {
    return wait(events, false, timeout);
}

bool manual_reset_event::reset() const {
    if(_handle == nullptr)
        return false;
    return ResetEvent(_handle) != 0;
}

manual_reset_event::manual_reset_event() : event_base(CreateEvent(nullptr, FALSE, FALSE, nullptr)) {}

auto_reset_event::auto_reset_event() : event_base(CreateEvent(nullptr, FALSE, FALSE, nullptr)) {}

PSECURITY_DESCRIPTOR make_allow_all_security_descriptor() {
    const WCHAR *pszStringSecurityDescriptor;
    pszStringSecurityDescriptor = L"D:(A;;GA;;;WD)(A;;GA;;;AN)S:(ML;;NW;;;ME)";
    PSECURITY_DESCRIPTOR pSecDesc;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptor(pszStringSecurityDescriptor, SDDL_REVISION_1, &pSecDesc, nullptr))
        return nullptr;

    return pSecDesc;
}

winapi_error::winapi_error(const char *message) : runtime_error(generate_message(message).c_str()) {}

std::string winapi_error::generate_message(const std::string &message) {
    std::stringstream ss;
    ss << message << " Last Error: " << last_error_string(GetLastError()) << std::endl;
    return ss.str();
}

std::string winapi_error::last_error_string(DWORD lastError) {
    // TODO: Error handling
    LPSTR  messageBuffer = nullptr;
    size_t size          = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError,
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

    std::string message(messageBuffer, size);

    LocalFree(messageBuffer);

    return message;
}

}  // namespace libobsensor

