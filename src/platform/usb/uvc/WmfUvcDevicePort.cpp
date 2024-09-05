#if(_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#include <ntverp.h>
#if VER_PRODUCTBUILD <= 9600  //(WinSDK 8.1)
#ifdef ENFORCE_METADATA
#error( "libobsensor Error!: Featuring UVC Metadata requires WinSDK 10.0.10586.0. \
 Install the required toolset to proceed. Alternatively, uncheck ENFORCE_METADATA propertyId in CMake GUI tool")
#else
#pragma message("\nlibobsensor Notification: Featuring UVC Metadata requires WinSDK 10.0.10586.0 toolset. \
The library will be compiled without the metadata support!\n")
#endif  // ENFORCE_METADATA
#else
#define METADATA_SUPPORT
#endif  // (WinSDK 8.1)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define DEVICE_ID_MAX_SIZE 256
#define WAIT_STREAM_TIMEOUT_MSEC 3000

#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "WmfUvcDevicePort.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfileFactory.hpp"

#include <algorithm>
#include <cassert>
#include <vector>
#include <chrono>
#include <cmath>
#include <limits>

#include <Shlwapi.h>
#include <Mferror.h>
#include <vidcap.h>
#include <ksmedia.h>  // Metadata Extension

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define did_guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK

#define DEVICE_NOT_READY_ERROR _HRESULT_TYPEDEF_(0x80070015L)

#define MAX_PINS 5
#define type_guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID

namespace libobsensor {

// convert to standard fourcc codes
const std::unordered_map<uint32_t, uint32_t> fourcc_map = {
    { 0x59382020, 0x47524559 }, /* 'GREY' from 'Y8  ' */
    { 0x52573130, 0x70524141 }, /* 'pRAA' from 'RW10'.*/
    { 0x32000000, 0x47524559 }, /* 'GREY' from 'L8  ' */
    { 0x50000000, 0x5a313620 }, /* 'Z16'  from 'D16 ' */
    { 0x52415738, 0x47524559 }, /* 'GREY' from 'RAW8' */
    { 0x52573136, 0x42595232 }  /* 'RW16' from 'BYR2' */
};

int exposure_value_to_exponent(int val) {  // exp-unit: 0.1ms
    return (int)(log((double)val / 10000.0) / log(2.0));
}

int exposure_exponent_to_value(int exponent) {
    return (int)(pow(2.0, (double)exponent) * 10000);
}

#ifdef METADATA_SUPPORT

#pragma pack(push, 1)
struct ms_proprietary_md_blob {
    // These fields are identical in layout and content with the standard UVC header
    uint32_t timestamp;
    uint8_t  source_clock[6];
    // MS internal
    uint8_t reserved[6];
};

struct ms_metadata_header {
    KSCAMERA_METADATA_ITEMHEADER ms_header;
    ms_proprietary_md_blob       ms_blobs[2];  // The blobs content is identical
};
#pragma pack(pop)

constexpr uint8_t ms_header_size = sizeof(ms_metadata_header);

// uint32_t tempTime = 0;

size_t try_read_metadata(IMFSample *pSample, UvcMetadata *metadata) {
    CComPtr<IUnknown>      spUnknown;
    CComPtr<IMFAttributes> spSample;
    HRESULT                hr = S_OK;

    CHECK_HR(hr = pSample->QueryInterface(IID_PPV_ARGS(&spSample)));
    LOG_HR(hr = spSample->GetUnknown(MFSampleExtension_CaptureMetadata, IID_PPV_ARGS(&spUnknown)));

    if(SUCCEEDED(hr)) {
        CComPtr<IMFAttributes>        spMetadata;
        CComPtr<IMFMediaBuffer>       spBuffer;
        PKSCAMERA_METADATA_ITEMHEADER pMetadata       = nullptr;
        DWORD                         dwMaxLength     = 0;
        DWORD                         dwCurrentLength = 0;

        CHECK_HR(hr = spUnknown->QueryInterface(IID_PPV_ARGS(&spMetadata)));
        if(spMetadata == nullptr)
            return false;

        hr = spMetadata->GetUnknown(MF_CAPTURE_METADATA_FRAME_RAWSTREAM, IID_PPV_ARGS(&spBuffer));
        LOG_HR(hr);
        if(spBuffer == nullptr || FAILED(hr))
            return false;

        CHECK_HR(hr = spBuffer->Lock(reinterpret_cast<BYTE **>(&pMetadata), &dwMaxLength, &dwCurrentLength));

        if(nullptr == pMetadata)  // Bail, no data.
            return 0;

        if(pMetadata->MetadataId != MetadataId_UsbVideoHeader)  // Wrong metadata type, bail.
            return 0;

        // Microsoft converts the standard UVC (12-byte) header into MS proprietary 40-bytes struct(ms_metadata_header)
        // Therefore we revert it to the original structure for uniform handling
        static constexpr uint8_t md_length_max = 0xff;  // Metadata for Bulk endpoints is limited to 255 bytes by design
        auto                     md_raw        = reinterpret_cast<byte *>(pMetadata);
        auto                    *ms_md_hdr     = reinterpret_cast<ms_metadata_header *>(md_raw);

        size_t extraDataSize = 0;
        if(ms_md_hdr->ms_header.Size > 0) {
            metadata->header.dwPresentationTime = ms_md_hdr->ms_blobs[0].timestamp;
            memcpy(metadata->header.scrSourceClock, ms_md_hdr->ms_blobs[0].source_clock, sizeof(metadata->header.scrSourceClock));
            extraDataSize = static_cast<uint8_t>(ms_md_hdr->ms_header.Size - ms_header_size);  // md_payload_size
            if(extraDataSize > md_length_max) {
                extraDataSize = md_length_max;
            }
            memcpy(metadata->metadata, md_raw + ms_header_size, extraDataSize);
        }

        // auto showUvcData = ( byte* )uvc_hdr;
        // for ( int i = 0; i < metadataSize + uvcHeaderSize; i++ ) {
        //    printf( "0x%.2x ", showUvcData[ i ] );
        //    if ( i % 16 == 15 ) {
        //        printf( "\n" );
        //    }
        //}
        // printf( "\n\n" );

        /*std::cout << "def Time=" << uvc_hdr->timesTamp - tempTime << std::endl;
        tempTime = uvc_hdr->timesTamp;*/
        // std::cout << "uvc_hdr->timesTamp" << uvc_hdr->timesTamp << std::endl;

        return extraDataSize;
    }
    else
        return 0;
}
#endif  // METADATA_SUPPORT

bool WmfUvcDevicePort::isConnected(std::shared_ptr<const USBSourcePortInfo> info) {
    auto result = false;
    foreachUvcDevice([&](const UsbInterfaceInfo &devInfo, IMFActivate *) {
        if(devInfo.infUrl == info->infUrl) {
            result = true;
        }
    });
    return result;
}

struct pu_control {
    uint32_t propertyId;
    long     ksProperty;
    bool     enable_auto;
};

static const pu_control pu_controls[] = { { OB_PROP_COLOR_BRIGHTNESS_INT, KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS },
                                          { OB_PROP_COLOR_CONTRAST_INT, KSPROPERTY_VIDEOPROCAMP_CONTRAST },
                                          { OB_PROP_COLOR_HUE_INT, KSPROPERTY_VIDEOPROCAMP_HUE },
                                          { OB_PROP_COLOR_SATURATION_INT, KSPROPERTY_VIDEOPROCAMP_SATURATION },
                                          { OB_PROP_COLOR_SHARPNESS_INT, KSPROPERTY_VIDEOPROCAMP_SHARPNESS },
                                          { OB_PROP_COLOR_GAMMA_INT, KSPROPERTY_VIDEOPROCAMP_GAMMA },
                                          { OB_PROP_COLOR_WHITE_BALANCE_INT, KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE },
                                          { OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, true },
                                          { OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, KSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION },
                                          { OB_PROP_COLOR_GAIN_INT, KSPROPERTY_VIDEOPROCAMP_GAIN },
                                          { OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY } };

// Camera Terminal controls will be handled with  PU propertyId transport and handling mechanism
static const pu_control ct_controls[] = {
    { OB_PROP_COLOR_ROLL_INT, KSPROPERTY_CAMERACONTROL_ROLL },
    { OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT, KSPROPERTY_CAMERACONTROL_AUTO_EXPOSURE_PRIORITY },
    { OB_PROP_COLOR_FOCUS_INT, KSPROPERTY_CAMERACONTROL_FOCUS },
};

bool WmfUvcDevicePort::getPu(uint32_t propertyId, int32_t &retValue) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    long val = 0, flags = 0;
    if((propertyId == OB_PROP_COLOR_EXPOSURE_INT) || (propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL)) {
        auto hr = getCameraControl()->Get(CameraControl_Exposure, &val, &flags);
        if(hr == DEVICE_NOT_READY_ERROR)
            return false;

        retValue = (propertyId == OB_PROP_COLOR_EXPOSURE_INT) ? exposure_exponent_to_value(val) : (flags == CameraControl_Flags_Auto);
        CHECK_HR(hr);
        return true;
    }

    for(auto &pu: pu_controls) {
        if(propertyId == pu.propertyId) {
            auto hr = getVideoProc()->Get(pu.ksProperty, &val, &flags);
            if(hr == DEVICE_NOT_READY_ERROR)
                return false;

            retValue = (pu.enable_auto) ? (flags == VideoProcAmp_Flags_Auto) : val;

            CHECK_HR(hr);
            return true;
        }
    }

    for(auto &ct: ct_controls) {
        if(propertyId == ct.propertyId) {
            auto hr = getCameraControl()->Get(ct.ksProperty, &val, &flags);
            if(hr == DEVICE_NOT_READY_ERROR)
                return false;

            retValue = val;

            CHECK_HR(hr);
            return true;
        }
    }

    throw std::runtime_error(utils::string::to_string() << "Unsupported control - " << propertyId);
}

bool WmfUvcDevicePort::setPu(uint32_t propertyId, int32_t tarValue) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    if(propertyId == OB_PROP_COLOR_EXPOSURE_INT) {
        auto hr = getCameraControl()->Set(CameraControl_Exposure, exposure_value_to_exponent(tarValue), CameraControl_Flags_Manual);
        if(hr == DEVICE_NOT_READY_ERROR)
            return false;

        CHECK_HR(hr);
        return true;
    }
    if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
        if(tarValue) {
            auto hr = getCameraControl()->Set(CameraControl_Exposure, 0, CameraControl_Flags_Auto);
            if(hr == DEVICE_NOT_READY_ERROR)
                return false;

            CHECK_HR(hr);
        }
        else {
            long min, max, step, def, caps;
            auto hr = getCameraControl()->GetRange(CameraControl_Exposure, &min, &max, &step, &def, &caps);
            if(hr == DEVICE_NOT_READY_ERROR)
                return false;

            CHECK_HR(hr);

            hr = getCameraControl()->Set(CameraControl_Exposure, def, CameraControl_Flags_Manual);
            if(hr == DEVICE_NOT_READY_ERROR)
                return false;

            CHECK_HR(hr);
        }
        return true;
    }

    for(auto &pu: pu_controls) {
        if(propertyId == pu.propertyId) {
            if(pu.enable_auto) {
                if(tarValue) {
                    auto hr = getVideoProc()->Set(pu.ksProperty, 0, VideoProcAmp_Flags_Auto);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
                else {
                    long min, max, step, def, caps;
                    auto hr = getVideoProc()->GetRange(pu.ksProperty, &min, &max, &step, &def, &caps);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);

                    hr = getVideoProc()->Set(pu.ksProperty, def, VideoProcAmp_Flags_Manual);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
            }
            else {
                auto hr = getVideoProc()->Set(pu.ksProperty, tarValue, VideoProcAmp_Flags_Manual);
                if(hr == DEVICE_NOT_READY_ERROR)
                    return false;

                CHECK_HR(hr);
            }
            return true;
        }
    }
    for(auto &ct: ct_controls) {
        if(propertyId == ct.propertyId) {
            if(ct.enable_auto) {
                if(tarValue) {
                    auto hr = getCameraControl()->Set(ct.ksProperty, 0, CameraControl_Flags_Auto);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
                else {
                    long min, max, step, def, caps;
                    auto hr = getCameraControl()->GetRange(ct.ksProperty, &min, &max, &step, &def, &caps);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);

                    hr = getCameraControl()->Set(ct.ksProperty, def, CameraControl_Flags_Manual);
                    if(hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
            }
            else {
                auto hr = getCameraControl()->Set(ct.ksProperty, tarValue, CameraControl_Flags_Manual);
                if(hr == DEVICE_NOT_READY_ERROR)
                    return false;

                CHECK_HR(hr);
            }
            return true;
        }
    }
    throw std::runtime_error(utils::string::to_string() << "Unsupported control - " << propertyId);
}

UvcControlRange WmfUvcDevicePort::getPuRange(uint32_t propertyId) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }

    if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL || propertyId == OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL) {
        static const int32_t min = 0, max = 1, step = 1, def = 1;
        UvcControlRange      result(min, max, step, def);
        return result;
    }

    long minVal = 0, maxVal = 0, steppingDelta = 0, defVal = 0, capsFlag = 0;
    if(propertyId == OB_PROP_COLOR_EXPOSURE_INT) {
        CHECK_HR(getCameraControl()->GetRange(CameraControl_Exposure, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
        long min = exposure_exponent_to_value(minVal);
        long max = exposure_exponent_to_value(maxVal);
        long def = exposure_exponent_to_value(defVal);

        UvcControlRange result(min, max, steppingDelta, def);
        return result;
    }
    for(auto &pu: pu_controls) {
        if(propertyId == pu.propertyId) {
            CHECK_HR(getVideoProc()->GetRange(pu.ksProperty, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
            UvcControlRange result(minVal, maxVal, steppingDelta, defVal);
            return result;
        }
    }
    for(auto &ct: ct_controls) {
        if(propertyId == ct.propertyId) {
            if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT) {
                minVal        = 0;
                maxVal        = 1;
                steppingDelta = 1;
                defVal        = 0;
            }
            else {
                CHECK_HR(getCameraControl()->GetRange(ct.ksProperty, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
            }
            UvcControlRange result(minVal, maxVal, steppingDelta, defVal);
            return result;
        }
    }
    throw std::runtime_error("unsupported control");
}

uint32_t WmfUvcDevicePort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    // checkConnection();
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    if(xuKsControl_ == nullptr) {
        initXu();
    }

    // sendData
    uint8_t ctrl         = OB_VENDOR_XU_CTRL_ID_64;
    auto    alignDataLen = sendLen;
    if(alignDataLen <= 64) {
        ctrl         = OB_VENDOR_XU_CTRL_ID_64;
        alignDataLen = 64;
    }
    else if(alignDataLen > 512) {
        ctrl         = OB_VENDOR_XU_CTRL_ID_1024;
        alignDataLen = 1024;
    }
    else {
        ctrl         = OB_VENDOR_XU_CTRL_ID_512;
        alignDataLen = 512;
    }

    if(!setXu(ctrl, sendData, alignDataLen)) {
        LOG_ERROR("setXu failed!");
        return 0;
    }

    // receiveData
    ctrl             = OB_VENDOR_XU_CTRL_ID_512;
    uint32_t recvLen = exceptedRecvLen;
    if(exceptedRecvLen <= 64) {
        ctrl    = OB_VENDOR_XU_CTRL_ID_64;
        recvLen = 64;
    }
    else if(exceptedRecvLen > 512) {
        ctrl    = OB_VENDOR_XU_CTRL_ID_1024;
        recvLen = 1024;
    }
    else {
        ctrl    = OB_VENDOR_XU_CTRL_ID_512;
        recvLen = 512;
    }
    if(!getXu(ctrl, recvData, &recvLen)) {
        LOG_ERROR("getXu failed!");
        return 0;
    }
    return recvLen;
}

// std::string wcharToString(wchar_t *wchar) {
//     wchar_t *wText = wchar;
//     DWORD    dwNum = WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, NULL, 0, NULL, FALSE);  // WideCharToMultiByte的运用
//     char *   psText;  // psText为char*的临时数组，作为赋值给std::string的中间变量
//     psText = new char[dwNum];
//     WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, psText, dwNum, NULL, FALSE);  // WideCharToMultiByte的再次运用
//     std::string szDst = psText;                                                  // std::string赋值
//     delete[] psText;                                                             // psText的清除
//     return szDst;
// }

void WmfUvcDevicePort::queryXuNodeId(const CComPtr<IKsTopologyInfo> &topologyInfo) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    DWORD                                 nNodes = 0;
    check("get_NumNodes", topologyInfo->get_NumNodes(&nNodes));
    for(DWORD i = 0; i < nNodes; i++) {
        GUID nodeType;
        CHECK_HR(topologyInfo->get_NodeType(i, &nodeType))
        if(nodeType == KSNODETYPE_DEV_SPECIFIC) {
            xuNodeId_ = static_cast<int>(i);
        }

        // DWORD cbName = 0;
        // auto  hr     = topologyInfo->get_NodeName( i, NULL, 0, &cbName );
        // if ( hr == __HRESULT_FROM_WIN32( ERROR_MORE_DATA ) ) {
        //     if ( cbName > sizeof( WCHAR ) ) {
        //         WCHAR* nodeName = new WCHAR[ cbName / sizeof( WCHAR ) ];
        //         if ( nodeName == NULL ) {
        //             hr = E_OUTOFMEMORY;
        //         }
        //         else {
        //             hr = topologyInfo->get_NodeName( i, nodeName, cbName, &cbName );

        //              LOG_INFO("IKsTopologyInfo->node@{}, name={}", i, wcharToString(nodeName));
        //             delete[] nodeName;
        //         }
        //     }
        // }
    }
}

void WmfUvcDevicePort::initXu() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    // Attempt to retrieve IKsControl
    CComPtr<IKsTopologyInfo> ks_topology_info = nullptr;
    CHECK_HR(deviceSource_->QueryInterface(__uuidof(IKsTopologyInfo), reinterpret_cast<void **>(&ks_topology_info)));

    // Query Xu Node Id
    queryXuNodeId(ks_topology_info);

    // KSNODETYPE_VIDEO_STREAMING
    CComPtr<IUnknown> unknown = nullptr;
    CHECK_HR(ks_topology_info->CreateNodeInstance(xuNodeId_, IID_IUnknown, reinterpret_cast<LPVOID *>(&unknown)));

    CHECK_HR(unknown->QueryInterface(__uuidof(IKsControl), reinterpret_cast<void **>(&xuKsControl_)));
    if(xuKsControl_ == nullptr) {
        throw std::runtime_error("Extension control can not be initialized!");
    }
}

bool WmfUvcDevicePort::setXu(uint8_t ctrl, const uint8_t *data, uint32_t len) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    KSP_NODE node;
    memset(&node, 0, sizeof(KSP_NODE));
    node.Property.Set   = reinterpret_cast<const GUID &>(xuUnit_.id);
    node.Property.Id    = ctrl;
    node.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
    node.NodeId         = xuNodeId_;

    ULONG bytes_received = 0;
    addTimeoutBarrier("setXu");

    // 库内部会先GET_LEN命令获取协议数据长度，并按照GET_LEN返回值发送对应长度数据。
    // 所以当len<GET_LEN(): 会返回错误， len>GET_LEN(): 会只发送前边GET_LEN()个字节的数据
    auto hr = xuKsControl_->KsProperty(reinterpret_cast<PKSPROPERTY>(&node), sizeof(KSP_NODE), (void *)data, len, &bytes_received);

    removeTimeoutBarrier();
    return LOG_HR(hr);
}

bool WmfUvcDevicePort::getXu(uint8_t ctrl, uint8_t *data, uint32_t *len) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD0) {
        setPowerStateD0();
    }

    KSP_NODE node;
    memset(&node, 0, sizeof(KSP_NODE));
    node.Property.Set   = reinterpret_cast<const GUID &>(xuUnit_.id);
    node.Property.Id    = ctrl;
    node.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
    node.NodeId         = xuNodeId_;

    ULONG bytes_received = 0;
    addTimeoutBarrier("getXu");

    // 库内部会先GET_LEN命令获取协议数据长度，设备可以返回的数据长度需要小于等于GET_LEN(),
    // 如果 XU_MAX_DATA_LENGTH<GET_LEN(): 会返回错误
    auto hr = xuKsControl_->KsProperty(reinterpret_cast<PKSPROPERTY>(&node), sizeof(node), data, XU_MAX_DATA_LENGTH, &bytes_received);
    *len    = bytes_received;

    removeTimeoutBarrier();
    return LOG_HR(hr);
}

void WmfUvcDevicePort::foreachUvcDevice(const USBDeviceInfoEnumCallback &action) {
    for(const auto &attributes_params_set: attributes_params) {
        CComPtr<IMFAttributes> pAttributes = nullptr;
        CHECK_HR(MFCreateAttributes(&pAttributes, 1));
        for(auto attribute_params: attributes_params_set) {
            CHECK_HR(pAttributes->SetGUID(attribute_params.first, attribute_params.second));
        }

        IMFActivate **ppDevices;
        UINT32        numDevices;
        CHECK_HR(MFEnumDeviceSources(pAttributes, &ppDevices, &numDevices));

        for(UINT32 i = 0; i < numDevices; ++i) {
            CComPtr<IMFActivate> pDevice;
            *&pDevice = ppDevices[i];

            WCHAR *wchar_name = nullptr;
            UINT32 length;
            CHECK_HR(pDevice->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wchar_name, &length));
            auto symbolicLink = win_to_utf(wchar_name);
            CoTaskMemFree(wchar_name);

            pDevice->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &wchar_name, &length);
            auto friendlyName = win_to_utf(wchar_name);
            //    LOG_DEBUG("device friendly name: "
            //              << friendlyName;
            CoTaskMemFree(wchar_name);

            uint16_t    vid, pid, infIndex;
            std::string uid, guid;
            if(parse_usb_path_multiple_interface(vid, pid, infIndex, uid, symbolicLink, guid) && vid == ORBBEC_USB_VID) {
                UsbInterfaceInfo info;
                // info.url = ""
                info.uid      = uid;
                info.vid      = vid;
                info.pid      = pid;
                info.infIndex = (uint8_t)infIndex;
                info.infUrl   = utils::string::toUpper(symbolicLink);
                info.infName  = friendlyName;
                TRY_EXECUTE({ action(info, ppDevices[i]); });
            }
        }
        safe_release(pAttributes);
        CoTaskMemFree(ppDevices);
    }
}

WmfUvcDevicePort::WmfUvcDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo) : portInfo_(portInfo), xuKsControl_(nullptr), xuNodeId_(0) {
    if(!isConnected(portInfo)) {
        throw std::runtime_error("Camera not connected!");
    }

    BEGIN_TRY_EXECUTE({
        UsbSpec     deviceUsbSpec;
        std::string location;
        std::string deviceSerial;
        std::string url;
        if(!getUsbDescriptors(portInfo->vid, portInfo->pid, portInfo->uid, location, deviceUsbSpec, deviceSerial, url)) {
            LOG_WARN("Could not retrieve USB descriptor for device {0:x}:{1:x} , url:{2}", portInfo->vid, portInfo->pid, portInfo->url);
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Accessing USB info failed for{0:x}:{1:x} , url:{2}", portInfo->vid, portInfo->pid, portInfo->url); })

    foreachUvcDevice([&](const UsbInterfaceInfo &devInfo, IMFActivate *device) {
        if(devInfo.infUrl == portInfo->infUrl && device) {
            deviceId_.resize(DEVICE_ID_MAX_SIZE);
            CHECK_HR(device->GetString(did_guid, const_cast<LPWSTR>(deviceId_.c_str()), static_cast<UINT32>(deviceId_.size()), nullptr));
        }
    });
    initTimeoutThread();
}

WmfUvcDevicePort::~WmfUvcDevicePort() noexcept {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    terminateTimeoutThread();
    TRY_EXECUTE({
        if(isStarted_) {
            stopAllStream();
            // flush(MF_SOURCE_READER_ALL_STREAMS);
        }
        if(powerState_ != kD3) {
            setPowerStateD3();
        }
        safe_release(deviceAttrs_);
        safe_release(readerAttrs_);
        if(xuKsControl_ != nullptr) {
            safe_release(xuKsControl_);
            xuKsControl_ = nullptr;
        }
    });
}

void WmfUvcDevicePort::initTimeoutThread() {
    timeoutThreadRun_ = true;
    timeoutThread_    = std::thread([&]() {
        while(timeoutThreadRun_) {
            std::unique_lock<std::mutex> lk(timeoutMutex_);
            if(!timeoutCondition_.wait_for(lk, std::chrono::seconds(10), [&]() { return !(timeoutThreadRun_ && hasTimeoutBarrier_); })) {
                if(!timeoutMsg_.empty()) {
                    LOG_ERROR("Catch timeout event.{}", timeoutMsg_);
                }
            }

            timeoutCondition_.wait(lk, [&]() { return !(timeoutThreadRun_ && !hasTimeoutBarrier_); });
        }
    });
}

void WmfUvcDevicePort::terminateTimeoutThread() {
    {
        std::unique_lock<std::mutex> lk(timeoutMutex_);
        timeoutThreadRun_  = false;
        hasTimeoutBarrier_ = false;
        timeoutMsg_        = "";
        timeoutCondition_.notify_all();
    }
    timeoutThread_.join();
}

void WmfUvcDevicePort::addTimeoutBarrier(std::string msg) {
    std::unique_lock<std::mutex> lk(timeoutMutex_);
    hasTimeoutBarrier_ = true;
    timeoutMsg_        = msg;
    timeoutCondition_.notify_all();
}

void WmfUvcDevicePort::removeTimeoutBarrier() {
    std::unique_lock<std::mutex> lk(timeoutMutex_);
    hasTimeoutBarrier_ = false;
    timeoutMsg_        = "";
    timeoutCondition_.notify_all();
}

std::shared_ptr<const SourcePortInfo> WmfUvcDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

CComPtr<IMFAttributes> WmfUvcDevicePort::createDeviceAttrs() {
    CComPtr<IMFAttributes> device_attrs = nullptr;

    CHECK_HR(MFCreateAttributes(&device_attrs, 2));
    CHECK_HR(device_attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, type_guid));
    CHECK_HR(device_attrs->SetString(did_guid, deviceId_.c_str()));
    return device_attrs;
}

CComPtr<IMFAttributes> WmfUvcDevicePort::createReaderAttrs() {
    CComPtr<IMFAttributes> reader_attrs = nullptr;
    CHECK_HR(MFCreateAttributes(&reader_attrs, 3));
    CHECK_HR(reader_attrs->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, FALSE));
    CHECK_HR(reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
    // for convert to rgb or rgba
    CHECK_HR(reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE));
    CHECK_HR(reader_attrs->SetUINT32(MF_XVP_DISABLE_FRC, TRUE));

    CHECK_HR(reader_attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, static_cast<IUnknown *>(this)));
    return reader_attrs;
}

void WmfUvcDevicePort::setPowerStateD0() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ == kD0) {
        return;
    }
    if(!deviceAttrs_) {
        deviceAttrs_ = createDeviceAttrs();
    }

    if(!readerAttrs_) {
        readerAttrs_ = createReaderAttrs();
    }

    // enable source
    CHECK_HR(MFCreateDeviceSource(deviceAttrs_, &deviceSource_));
    LOG_HR(deviceSource_->QueryInterface(__uuidof(IAMCameraControl), reinterpret_cast<void **>(&cameraControl_)));
    LOG_HR(deviceSource_->QueryInterface(__uuidof(IAMVideoProcAmp), reinterpret_cast<void **>(&videoProc_)));

    // enable reader
    CHECK_HR(MFCreateSourceReaderFromMediaSource(deviceSource_, readerAttrs_, &streamReader_));
    // CHECK_HR(streamReader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), TRUE));
    powerState_ = kD0;
}

void WmfUvcDevicePort::setPowerStateD3() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ == kD3) {
        return;
    }
    safe_release(cameraControl_);
    safe_release(videoProc_);
    // safe_release(streamReader_);
    streamReader_.Reset();

    if(deviceSource_ != nullptr) {
        deviceSource_->Shutdown();  // Failure to call Shutdown can result in memory leak
        safe_release(deviceSource_);
    }

    if(xuKsControl_ != nullptr) {
        safe_release(xuKsControl_);
        xuKsControl_ = nullptr;
    }

    streams_.clear();
    powerState_ = kD3;
}

void WmfUvcDevicePort::reloadSourceAndReader() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    if(powerState_ != kD3) {
        setPowerStateD3();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    setPowerStateD0();
}

StreamProfileList WmfUvcDevicePort::getStreamProfileList() {
    if(profileList_.empty()) {
        std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
        checkConnection();
        bool recoveryD3 = false;
        if(powerState_ != kD0) {
            setPowerStateD0();
            recoveryD3 = true;
        }
        foreachProfile([&](const MFProfile &mfp, CComPtr<IMFMediaType> media_type, bool &quit) {
            utils::unusedVar(media_type);
            utils::unusedVar(quit);
            profileList_.push_back(mfp.profile);
            if(mfp.profile->getFormat() == OB_FORMAT_NV12) {
                std::shared_ptr<StreamProfile> sp         = mfp.profile;
                auto bgrProfile = sp->clone(OB_FORMAT_BGR)->as<VideoStreamProfile>();
                profileList_.push_back(bgrProfile);
                auto bgraProfile = sp->clone(OB_FORMAT_BGRA)->as<VideoStreamProfile>();
                profileList_.push_back(bgraProfile);
            }
        });
        if(recoveryD3) {
            setPowerStateD3();
        }
    }
    return profileList_;
}

void WmfUvcDevicePort::foreachProfile(std::function<void(const MFProfile &profile, CComPtr<IMFMediaType> media_type, bool &quit)> action) const {
    bool                  quit       = false;
    CComPtr<IMFMediaType> pMediaType = nullptr;
    for(unsigned int index = 0; index < MAX_PINS; ++index) {
        for(auto k = 0;; k++) {
            auto hr = streamReader_->GetNativeMediaType(index, k, &pMediaType.p);
            if(FAILED(hr) || pMediaType == nullptr) {
                safe_release(pMediaType);
                if(hr != MF_E_NO_MORE_TYPES)  // An object ran out of media types to suggest therefore the requested chain of streaming objects cannot be
                                              // completed
                    check("streamReader_->GetNativeMediaType(index, k, &pMediaType.p)", hr, false);

                break;
            }

            GUID subtype;
            CHECK_HR(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype));

            uint32_t width  = 0;
            uint32_t height = 0;

            CHECK_HR(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height));

            FrameRate frameRateMin;
            FrameRate frameRateMax;

            CHECK_HR(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE_RANGE_MIN, &frameRateMin.numerator, &frameRateMin.denominator));
            CHECK_HR(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE_RANGE_MAX, &frameRateMax.numerator, &frameRateMax.denominator));

            if(static_cast<float>(frameRateMax.numerator) / frameRateMax.denominator < static_cast<float>(frameRateMin.numerator) / frameRateMin.denominator) {
                std::swap(frameRateMax, frameRateMin);
            }
            auto currFps = static_cast<uint16_t>(frameRateMax.numerator / frameRateMax.denominator);

            uint32_t device_fourcc = reinterpret_cast<const utils::big_endian<uint32_t> &>(subtype.Data1);

            auto format = utils::uvcFourccToOBFormat(device_fourcc);
            if(format != OB_FORMAT_UNKNOWN) {
                std::shared_ptr<StreamProfile> sp = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_VIDEO, format, width, height, currFps);
                sp->setIndex(static_cast<uint8_t>(index));

                MFProfile mfp;
                mfp.index    = index;
                mfp.min_rate = frameRateMin;
                mfp.max_rate = frameRateMax;
                mfp.profile  = sp->as<VideoStreamProfile>();

                action(mfp, pMediaType, quit);
            }

            safe_release(pMediaType);
            // safe_release(spOutputMediaType);

            if(quit)
                return;
        }
    }
}

void WmfUvcDevicePort::playProfile(std::shared_ptr<const VideoStreamProfile> profile, MutableFrameCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    bool                                  found = false;
    foreachProfile([&](const MFProfile &mfp, CComPtr<IMFMediaType> media_type, bool &quit) {
        if(mfp.profile->getWidth() != profile->getWidth() || mfp.profile->getHeight() != profile->getHeight() || mfp.profile->getFps() != profile->getFps()) {
            return;
        }

        if((profile->getFormat() == OB_FORMAT_BGR || profile->getFormat() == OB_FORMAT_BGRA) && mfp.profile->getFormat() == OB_FORMAT_NV12) {
            CComPtr<IMFMediaType> spOutputMediaType = nullptr;
            // Create Output Media type
            {
                Microsoft::WRL::ComPtr<IMFSourceReaderEx> spSourceReaderEx;
                CHECK_HR(streamReader_.As(&spSourceReaderEx));
                DWORD dwStreamFlags = 0;
                CHECK_HR(spSourceReaderEx->SetNativeMediaType(mfp.index, media_type, &dwStreamFlags));
                CHECK_HR(MFCreateMediaType(&spOutputMediaType));
                CHECK_HR(media_type->CopyAllItems(spOutputMediaType));
                if(profile->getFormat() == OB_FORMAT_BGR) {
                    CHECK_HR(spOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24));  // the actual format is BGR24
                }
                else if(profile->getFormat() == OB_FORMAT_BGRA) {
                    CHECK_HR(spOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32));  // the actual format is BGRA32
                }
                media_type = spOutputMediaType;
            }
        }
        else if(mfp.profile->getFormat() != profile->getFormat()) {
            return;
        }

        auto hr = streamReader_->SetCurrentMediaType(mfp.index, nullptr, media_type);
        if(SUCCEEDED(hr) && media_type) {
            for(unsigned int i = 0; i < 5; ++i) {
                auto &stream = streams_[mfp.index];
                if((mfp.index == i) || (stream.callback))
                    continue;

                streamReader_->SetStreamSelection(i, FALSE);
            }

            CHECK_HR(streamReader_->SetStreamSelection(mfp.index, TRUE));
            {
                std::lock_guard<std::mutex> lock(streamsMutex_);
                auto                       &stream = streams_[mfp.index];  // std::map特性，如果不存在会自动创建
                if(streams_[mfp.index].streaming) {
                    throw std::runtime_error("Camera already streaming via this stream index!");
                }
                stream.profile   = profile;
                stream.callback  = callback;
                stream.streaming = true;
                stream.hasStarted.reset();
                stream.frameCounter = 0;

                isStarted_        = true;
                readSampleResult_ = S_OK;
                CHECK_HR(streamReader_->ReadSample(mfp.index, 0, nullptr, nullptr, nullptr, nullptr));
            }

            if(streams_[mfp.index].hasStarted.wait(WAIT_STREAM_TIMEOUT_MSEC)) {
                check("streamReader_->ReadSample(...)", readSampleResult_);
            }
            else {
                LOG_WARN("First frame took more than {}ms to arrive!", WAIT_STREAM_TIMEOUT_MSEC);
            }

            found = true;
            quit  = true;
            return;
        }
        else {
            throw std::runtime_error("Could not set Media Type. Device may be locked");
        }
    });
    if(!found)
        throw std::runtime_error("Stream profile not found!");
}

IAMVideoProcAmp *WmfUvcDevicePort::getVideoProc() const {
    if(powerState_ != kD0)
        throw std::runtime_error("Device must be powered to query video_proc!");
    if(!videoProc_.p)
        throw std::runtime_error("The device does not support adjusting the qualities of an incoming video signal, such as brightness, contrast, hue, "
                                 "saturation, gamma, and sharpness.");
    return videoProc_.p;
}

IAMCameraControl *WmfUvcDevicePort::getCameraControl() const {
    if(powerState_ != kD0)
        throw std::runtime_error("Device must be powered to query camera_control!");
    if(!cameraControl_.p)
        throw std::runtime_error("The device does not support camera settings such as zoom, pan, aperture adjustment, or shutter speed.");
    return cameraControl_.p;
}

void WmfUvcDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    checkConnection();
    if(powerState_ != kD0) {
        setPowerStateD0();
    }
    auto vsp = profile->as<VideoStreamProfile>();
    BEGIN_TRY_EXECUTE({ playProfile(vsp, callback); })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("Retry to start stream!");
        TRY_EXECUTE(stopAllStream());
        reloadSourceAndReader();
        playProfile(vsp, callback);
    })
}

void WmfUvcDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    bool                                  isStarted = false;  // 会重新根据是否还有流在出流状态赋值
    for(auto &item: streams_) {
        auto &stream = item.second;
        if(stream.profile == profile && stream.streaming) {
            stream.streaming = false;
            stream.isFlushed.reset();
            BEGIN_TRY_EXECUTE(flush(item.first))
            CATCH_EXCEPTION_AND_EXECUTE({
                setPowerStateD3();
                throw;  // rethrow
            })
            if(isConnected(portInfo_) && !stream.isFlushed.wait(WAIT_STREAM_TIMEOUT_MSEC)) {
                LOG_WARN("Wait for flush more than {}ms!", WAIT_STREAM_TIMEOUT_MSEC);
            }
        }
        if(stream.streaming) {
            isStarted = true;  // 还有其他流在开着
        }
    }
    checkConnection();
    isStarted_ = isStarted;
    if(!isStarted_ && powerState_ != kD3) {
        setPowerStateD3();
    }
}

void WmfUvcDevicePort::stopAllStream() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    checkConnection();
    isStarted_ = false;
    for(auto &item: streams_) {
        auto &stream     = item.second;
        stream.streaming = false;
        stream.isFlushed.reset();
        flush(item.first);
        if(!stream.isFlushed.wait(WAIT_STREAM_TIMEOUT_MSEC)) {
            LOG_WARN("Wait for flush more than {}ms!", WAIT_STREAM_TIMEOUT_MSEC);
        }
    }
    if(powerState_ != kD3) {
        setPowerStateD3();
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void WmfUvcDevicePort::flush(int index) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);  // flush 是同步的
    if(isConnected(portInfo_)) {
        if(streamReader_ != nullptr) {
            CHECK_HR(streamReader_->Flush(index));  // flush同时会关闭掉流
        }
    }
}

void WmfUvcDevicePort::checkConnection() const {
    if(!isConnected(portInfo_)) {
        throw std::runtime_error("Camera is no longer connected!");
    }
}

STDMETHODIMP WmfUvcDevicePort::QueryInterface(REFIID iid, void **ppv) {
#pragma warning(push)
#pragma warning(disable : 4838)
    static const QITAB qit[] = {
        QITABENT(WmfUvcDevicePort, IMFSourceReaderCallback),
        { nullptr },
    };
    return QISearch(this, qit, iid, ppv);
#pragma warning(pop)
};

STDMETHODIMP_(ULONG) WmfUvcDevicePort::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) WmfUvcDevicePort::Release() {
    ULONG count = InterlockedDecrement(&refCount_);
    // if(count <= 0) {
    //     delete this;
    // }
    return count;
}

STDMETHODIMP WmfUvcDevicePort::OnReadSample(HRESULT hrStatus, DWORD streamIndex, DWORD /*dwStreamFlags*/, LONGLONG /*llTimestamp*/, IMFSample *sample) {
    if(streamReader_) {
        if(FAILED(hrStatus)) {
            readSampleResult_ = hrStatus;
        }
        {
            std::lock_guard<std::mutex> lock(streamsMutex_);
            auto                       &stream = streams_[streamIndex];
            stream.hasStarted.set();

            if(!stream.streaming || !isStarted_) {
                return S_OK;
            }
        }
        LOG_HR(streamReader_->ReadSample(streamIndex, 0, nullptr, nullptr, nullptr, nullptr));  // 触发下一帧数据获取

        if(sample) {
            CComPtr<IMFMediaBuffer> buffer = nullptr;
            if(SUCCEEDED(sample->GetBufferByIndex(0, &buffer))) {
                byte *byte_buffer = nullptr;
                DWORD max_length{}, current_length{};
                if(SUCCEEDED(buffer->Lock(&byte_buffer, &max_length, &current_length))) {
                    std::lock_guard<std::mutex> lock(streamsMutex_);
                    auto                       &stream = streams_[streamIndex];
                    auto                        frame  = FrameFactory::createFrameFromStreamProfile(stream.profile);
                    auto                        vsp    = frame->as<VideoFrame>();

                    stream.frameCounter++;
                    vsp->setNumber(stream.frameCounter);

                    auto realtime = utils::getNowTimesUs();
                    vsp->setSystemTimeStampUsec(realtime);

                    auto metadata                     = frame->getMetadataMutable();
                    auto uvcMetadata                  = reinterpret_cast<UvcMetadata *>(metadata);
                    uvcMetadata->header.bHeaderLength = 12;
                    uvcMetadata->header.bmHeaderInfo  = 0;  // not used

#ifdef METADATA_SUPPORT
                    auto metadataExtraSize = try_read_metadata(sample, uvcMetadata);
                    /*LOG( INFO ) << "metadataSize=" << ( int )metadataSize;
                    for ( int i = 0; i < metadataSize; i++ ) {
                        printf( "0x%02x ", metadata[ i ] );
                        if ( i % 16 == 15 ) {
                            printf( "\n" );
                        }
                    }
                    printf( "\n\n" );*/
                    vsp->setTimeStampUsec(uvcMetadata->header.dwPresentationTime);
                    vsp->setMetadataSize(uvcMetadata->header.bHeaderLength + metadataExtraSize);
#endif

                    vsp->updateData(byte_buffer, current_length);

                    TRY_EXECUTE(stream.callback(frame));
                    buffer->Unlock();
                }
            }
        }
    }

    return S_OK;
}

STDMETHODIMP WmfUvcDevicePort::OnEvent(DWORD /*sidx*/, IMFMediaEvent * /*event*/) {
    return S_OK;
}

STDMETHODIMP WmfUvcDevicePort::OnFlush(DWORD streamIndex) {
    streamReader_->SetStreamSelection(streamIndex, false);
    auto &stream = streams_[streamIndex];
    stream.isFlushed.set();
    return S_OK;
}

}  // namespace libobsensor
