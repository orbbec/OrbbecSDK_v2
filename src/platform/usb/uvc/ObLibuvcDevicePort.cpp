// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObLibuvcDevicePort.hpp"
#include "UvcTypes.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

#include <utlist.h>
#include <libuvc/libuvc_internal.h>
#include <algorithm>

#define UVC_AE_MODE_D0_MANUAL (1 << 0)
#define UVC_AE_MODE_D1_AUTO (1 << 1)
#define UVC_AE_MODE_D2_SP (1 << 2)
#define UVC_AE_MODE_D3_AP (1 << 3)

#ifdef __ANDROID__
#include "pal/android/AndroidUsbDeviceManager.hpp"
#endif

namespace libobsensor {
using utils::fourCc2Int;
const std::map<uint32_t, uvc_frame_format> fourccToUvcFormatMap = {
    { fourCc2Int('U', 'Y', 'V', 'Y'), UVC_FRAME_FORMAT_UYVY }, { fourCc2Int('Y', 'U', 'Y', '2'), UVC_FRAME_FORMAT_YUYV },
    { fourCc2Int('N', 'V', '1', '2'), UVC_FRAME_FORMAT_NV12 }, { fourCc2Int('I', '4', '2', '0'), UVC_FRAME_FORMAT_I420 },
    { fourCc2Int('N', 'V', '2', '1'), UVC_FRAME_FORMAT_NV21 }, { fourCc2Int('M', 'J', 'P', 'G'), UVC_FRAME_FORMAT_MJPEG },
    { fourCc2Int('H', '2', '6', '4'), UVC_FRAME_FORMAT_H264 }, { fourCc2Int('H', 'E', 'V', 'C'), UVC_FRAME_FORMAT_HEVC },
    { fourCc2Int('Y', '8', ' ', ' '), UVC_FRAME_FORMAT_Y8 },   { fourCc2Int('Y', '1', '0', ' '), UVC_FRAME_FORMAT_Y10 },
    { fourCc2Int('Y', '1', '1', ' '), UVC_FRAME_FORMAT_Y11 },  { fourCc2Int('Y', '1', '2', ' '), UVC_FRAME_FORMAT_Y12 },
    { fourCc2Int('Y', '1', '4', ' '), UVC_FRAME_FORMAT_Y14 },  { fourCc2Int('Y', '1', '6', ' '), UVC_FRAME_FORMAT_Y16 },
    { fourCc2Int('R', 'V', 'L', ' '), UVC_FRAME_FORMAT_RVL },  { fourCc2Int('Z', '1', '6', ' '), UVC_FRAME_FORMAT_Z16 },
    { fourCc2Int('Y', 'V', '1', '2'), UVC_FRAME_FORMAT_YV12 }, { fourCc2Int('B', 'A', '8', '1'), UVC_FRAME_FORMAT_BA81 },
};

uvc_frame_format fourCC2UvcFormat(int32_t fourccCode) {
    uvc_frame_format uvcFormat = UVC_FRAME_FORMAT_UNKNOWN;
    std::find_if(fourccToUvcFormatMap.begin(), fourccToUvcFormatMap.end(), [&](const std::pair<uint32_t, uvc_frame_format> sf) {
        if(static_cast<uint32_t>(fourccCode) == sf.first) {
            uvcFormat = sf.second;
            return true;
        }
        return false;
    });
    return uvcFormat;
}

ObLibuvcDevicePort::ObLibuvcDevicePort(std::shared_ptr<IUsbDevice> usbDev, std::shared_ptr<const USBSourcePortInfo> portInfo)
    : usbDev_(usbDev), portInfo_(portInfo) {
    auto libusbDev       = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDev_);
    auto libusbCtx       = libusbDev->getLibusbContext();
    auto libusbDevHandle = libusbDev->getLibusbDeviceHandle();
    uvc_init(&uvcCtx_, libusbCtx);
    uvcDev_          = (uvc_device_t *)malloc(sizeof(*uvcDev_));
    uvcDev_->ctx     = uvcCtx_;
    uvcDev_->ref     = 0;
    uvcDev_->usb_dev = libusb_get_device(libusbDevHandle);

    auto res = uvc_open(uvcDev_, portInfo->infIndex, &uvcDevHandle_, libusbDevHandle);
    if(res < 0) {
        std::stringstream ss;
        ss << "uvc_open failed: [Path: " << portInfo->infUrl << ", Return Code: " << res << "]";
        LOG_WARN(ss.str());
        throw std::runtime_error(ss.str());
    }
    else {
        LOG_DEBUG("uvc_open success");
    }
}

ObLibuvcDevicePort::~ObLibuvcDevicePort() noexcept {
    LOG_DEBUG("~ObLibuvcDevicePort");
    stopAllStream();

    // free(uvcDev_); // free in uvc_exit()
    uvc_close(uvcDevHandle_);
    uvc_exit(uvcCtx_);

    LOG_INFO("uvc_close done.");
    LOG_INFO("~ObLibuvcDevicePort done");
}

void ObLibuvcDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, libobsensor::MutableFrameCallback callback) {
    LOG_DEBUG("ObLibuvcDevicePort::startStream()...");
    bool       foundProfile       = false;
    uvcProfile selectedUvcProfile = { 0 };
    auto       uvcProfiles        = queryAvailableUvcProfile();
    auto       videoProfile       = std::dynamic_pointer_cast<const VideoStreamProfile>(profile);
    for(auto &&pf: uvcProfiles) {
        auto obFormat = utils::uvcFourccToOBFormat(pf.fourcc);
        if(utils::uvcFourccToOBFormat(pf.fourcc) == OB_FORMAT_UNKNOWN) {
            continue;
        }
        if((videoProfile->getFormat() == obFormat) && (videoProfile->getFps() == pf.fps) && (videoProfile->getHeight() == pf.height)
           && (videoProfile->getWidth() == pf.width)) {
            foundProfile       = true;
            selectedUvcProfile = pf;
            break;
        }
    }
    LOG_DEBUG("playProfile: infIndex={0}, selected_format.width={1}, height={2}, format={3}", (uint32_t)portInfo_->infIndex, videoProfile->getWidth(),
              videoProfile->getHeight(), videoProfile->getFormat());

    if(!foundProfile) {
        throw std::runtime_error("Failed to find supported format!");
    }

    // libusb_clear_halt(uvcDevHandle_->usb_devh, selectedUvcProfile.endpointAddress);

    uvc_stream_ctrl_t ctrl;
    auto              res = uvc_get_stream_ctrl_format_size(uvcDevHandle_, &ctrl, fourCC2UvcFormat(selectedUvcProfile.fourcc), videoProfile->getWidth(),
                                                            videoProfile->getHeight(), videoProfile->getFps());
    if(res < 0) {
        LOG_ERROR("uvc_get_stream_ctrl_format_size failed!");
        throw std::runtime_error("uvc_get_stream_ctrl_format_size failed!");
    }

    uvc_stream_handle_t *uvcStreamHandle = nullptr;
    uvc_error_t          ret             = uvc_stream_open_ctrl(uvcDevHandle_, &uvcStreamHandle, &ctrl);
    if(ret != UVC_SUCCESS) {
        throw std::runtime_error("uvc_stream_open_ctrl failed!");
    }

    {
        std::unique_lock<std::mutex> lock(streamMutex_);
        int32_t                      bufNum = LIBUVC_NUM_TRANSFER_BUFS;
        if((profile->getFormat() == OB_FORMAT_MJPG || videoProfile->getFormat() == OB_FORMAT_Y8) && videoProfile->getFps() <= LIBUVC_TRANSFER_LOW_FRAME_SIZE) {
            bufNum = LIBUVC_NUM_TRANSFER_LOW_FRAME_BUFS;
        }
        auto obStreamHandle = std::make_shared<OBUvcStreamHandle>(videoProfile, callback, uvcStreamHandle);
        // std::shared_ptr<OBUvcStreamHandle>(new OBUvcStreamHandle(profile, callback, uvcStreamHandle));
        streamHandles_.push_back(obStreamHandle);
        uvcStreamHandle->actual_transfer_buff_num = bufNum;
        ret                                       = uvc_stream_start(uvcStreamHandle, ObLibuvcDevicePort::onFrameCallback, obStreamHandle.get(), 0);
    }

    if(ret == UVC_ERROR_NO_MEM) {
        for(uint32_t i = 0; i < uvcStreamHandle->actual_transfer_buff_num; i++) {
            if(uvcStreamHandle->transfers[i] != nullptr) {
                free(uvcStreamHandle->transfers[i]->buffer);
                libusb_free_transfer(uvcStreamHandle->transfers[i]);
                uvcStreamHandle->transfers[i] = nullptr;
            }
        }
        std::unique_lock<std::mutex> lock(streamMutex_);
        streamHandles_.erase(streamHandles_.end() - 1);
        throw std::runtime_error("uvc_stream_start failed with err_code=UVC_ERROR_NO_MEM, try to increase the usbfs buffer size!");
    }

    if(ret != UVC_SUCCESS) {
        std::unique_lock<std::mutex> lock(streamMutex_);
        streamHandles_.erase(streamHandles_.end() - 1);
        uvc_stream_close(uvcStreamHandle);
        throw std::runtime_error("uvc_stream_start failed!");
    }

    LOG_DEBUG("ObLibuvcDevicePort::startStream() done");
}

void ObLibuvcDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {

    LOG_DEBUG("ObLibuvcDevicePort::stopStream()...");
    std::unique_lock<std::mutex> lock(streamMutex_);
    auto                         videoProfile = std::dynamic_pointer_cast<const VideoStreamProfile>(profile);
    auto                         it           = std::find_if(streamHandles_.begin(), streamHandles_.end(), [&](const std::shared_ptr<OBUvcStreamHandle> sh) {
        return (videoProfile->getFormat() == sh->profile->getFormat()) && (videoProfile->getFps() == sh->profile->getFps())
               && (videoProfile->getHeight() == sh->profile->getHeight()) && (videoProfile->getWidth() == sh->profile->getWidth());
    });
    if(it == streamHandles_.end()) {
        LOG_DEBUG("can not find match stream handle.");
        return;
    }

    uvc_stream_handle_t *uvcStreamHandle = (*it)->streamHandle;
    auto                 endpointAddr    = uvcStreamHandle->stream_if->bEndpointAddress;
#ifdef OS_MACOS
    libusb_clear_halt(uvcDevHandle_->usb_devh, endpointAddr);
#endif
    uvc_stream_stop(uvcStreamHandle);
    uvc_stream_close(uvcStreamHandle);

#ifndef OS_MACOS
    libusb_clear_halt(uvcDevHandle_->usb_devh, endpointAddr);
#endif

    streamHandles_.erase(it);
    LOG_DEBUG("ObLibuvcDevicePort::stopStream() done");
}

void ObLibuvcDevicePort::stopAllStream() {
    std::unique_lock<std::mutex> lock(streamMutex_);
    if(streamHandles_.empty()) {
        return;
    }
    for(auto &&sh: streamHandles_) {
        uvc_stream_handle_t *uvcStreamHandle = sh->streamHandle;
        auto                 endpointAddr    = uvcStreamHandle->stream_if->bEndpointAddress;
        uvc_stream_stop(uvcStreamHandle);
        uvc_stream_close(uvcStreamHandle);
        auto ret = libusb_clear_halt(uvcDevHandle_->usb_devh, endpointAddr);
        if(ret != LIBUSB_SUCCESS) {
            LOG_ERROR("libusb_clear_halt failed, error code={}", ret);
        }
    }
    streamHandles_.clear();
    LOG_DEBUG("ObLibuvcDevicePort::stopAllStream() done");
}

bool ObLibuvcDevicePort::getPu(uint32_t propertyId, int32_t &value) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);
    int                                   unit    = 0;
    int                                   control = obPropToUvcCS(static_cast<OBPropertyID>(propertyId), unit);
    value                                         = getCtrl(UVC_GET_CUR, control, unit);
    value                                         = uvcCtrlValueTranslate(UVC_GET_CUR, static_cast<OBPropertyID>(propertyId), value);

    return true;
}

bool ObLibuvcDevicePort::setPu(uint32_t propertyId, int32_t value) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);
    int                                   unit;
    int                                   control = obPropToUvcCS(static_cast<OBPropertyID>(propertyId), unit);
    value                                         = uvcCtrlValueTranslate(UVC_SET_CUR, static_cast<OBPropertyID>(propertyId), value);
    setCtrl(UVC_SET_CUR, control, unit, value);
    LOG_DEBUG("ObLibuvcDevicePort::setPu() propertyId = {} value = ", propertyId, value);
    return true;
}

UvcControlRange ObLibuvcDevicePort::getPuRange(uint32_t propertyId) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);
    int                                   unit = 0;
    int                                   min, max, step, def;

    int control = obPropToUvcCS(static_cast<OBPropertyID>(propertyId), unit);
    if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT || propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL
       || propertyId == OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL) {
        min  = 0;
        max  = 1;
        step = 1;
    }
    else {
        min = getCtrl(UVC_GET_MIN, control, unit);
        min = uvcCtrlValueTranslate(UVC_GET_MIN, static_cast<OBPropertyID>(propertyId), min);

        max = getCtrl(UVC_GET_MAX, control, unit);
        max = uvcCtrlValueTranslate(UVC_GET_MAX, static_cast<OBPropertyID>(propertyId), max);

        step = getCtrl(UVC_GET_RES, control, unit);
        step = uvcCtrlValueTranslate(UVC_GET_RES, static_cast<OBPropertyID>(propertyId), step);
    }

    if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT) {
        def = 0;
    }
    else {
        def = getCtrl(UVC_GET_DEF, control, unit);
        def = uvcCtrlValueTranslate(UVC_GET_DEF, static_cast<OBPropertyID>(propertyId), def);
    }

    LOG_DEBUG("getPuRange unit={0}, propertyId={1}, min={2}, max={3}, step={4}, def={5}", unit, propertyId, min, max, step, def);
    if(step == min) {
        step = 1;
    }
    UvcControlRange result(min, max, step, def);

    return result;
}

bool ObLibuvcDevicePort::setXu(uint8_t ctrl, const uint8_t *data, uint32_t len) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);
    auto                                  recv = uvc_set_ctrl(uvcDevHandle_, xuUnit_.unit, ctrl, const_cast<uint8_t *>(data), len);
    if(recv <= 0) {
        LOG_ERROR("setXu failed, error code={}", recv);
        return false;
    }
    return true;
}

bool ObLibuvcDevicePort::getXu(uint8_t ctrl, uint8_t *data, uint32_t *len) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);
    switch((ObVendorXuCtrlId)ctrl) {
    case OB_VENDOR_XU_CTRL_ID_512:
        *len = 512;
        break;
    case OB_VENDOR_XU_CTRL_ID_64:
        *len = 64;
        break;

    case OB_VENDOR_XU_CTRL_ID_1024:
        *len = 1024;
        break;
    default:
        return false;
    }
    int recv = uvc_get_ctrl(uvcDevHandle_, xuUnit_.unit, ctrl, data, *len, UVC_GET_CUR);
    *len     = recv;
    if(recv <= 0) {
        LOG_ERROR("getXu failed, error code={}", recv);
        return false;
    }
    return true;
}

uint32_t ObLibuvcDevicePort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) {
    std::lock_guard<std::recursive_mutex> lock(ctrlMutex_);

    uint8_t ctrl = OB_VENDOR_XU_CTRL_ID_64;

    auto alignDataLen = sendLen;
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
        return 0;
    }

    ctrl = OB_VENDOR_XU_CTRL_ID_512;
    if(exceptedRecvLen <= 64) {
        ctrl = OB_VENDOR_XU_CTRL_ID_64;
    }
    else if(exceptedRecvLen > 512) {
        ctrl = OB_VENDOR_XU_CTRL_ID_1024;
    }
    else {
        ctrl = OB_VENDOR_XU_CTRL_ID_512;
    }

    if(!getXu(ctrl, recvData, &exceptedRecvLen)) {
        return 0;
    }
    return exceptedRecvLen;
}

StreamProfileList ObLibuvcDevicePort::getStreamProfileList() {
    StreamProfileList results;

    auto uvcProfiles = queryAvailableUvcProfile();
    for(auto &&pf: uvcProfiles) {
        auto obFormat = utils::uvcFourccToOBFormat(pf.fourcc);
        if(obFormat == OB_FORMAT_UNKNOWN) {
            continue;
        }
        // FIXME:
        auto sp = std::make_shared<VideoStreamProfile>(std::shared_ptr<LazySensor>(), OB_STREAM_VIDEO, obFormat, pf.width, pf.height, pf.fps);
        results.push_back(sp);
    }
    return results;
}

int32_t ObLibuvcDevicePort::uvcCtrlValueTranslate(uvc_req_code action, OBPropertyID propertyId, int32_t value) const {
    LOG_DEBUG("ObLibuvcDevicePort::uvcCtrlValueTranslate propertyId={0}, action={1}, value={2}", static_cast<int>(action), static_cast<int>(propertyId), value);
    // Value may be translated according to action/propertyId value
    int32_t translated_value = value;

    switch(action) {
    case UVC_GET_CUR:  // Translating from UVC 1.5 Spec up to RS
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            auto res = getCtrl(UVC_GET_RES, UVC_CT_AE_MODE_CONTROL, uvc_get_input_terminals(uvcDevHandle_)->bTerminalID);
            LOG_DEBUG("UVC_GET_CUR:getAERes:{}.", res);
            if(res & UVC_AE_MODE_D0_MANUAL) {
                translated_value = (value == UVC_AE_MODE_D0_MANUAL) ? 0 : 1;
                LOG_DEBUG("UVC_AE_MODE_D0_MANUAL:translated_value:{},value:{}.", translated_value, value);
            }
            else if(res & UVC_AE_MODE_D1_AUTO) {
                translated_value = (value == UVC_AE_MODE_D1_AUTO) ? 1 : 0;
                LOG_DEBUG("UVC_AE_MODE_D1_AUTO:translated_value:{},value:{}.", translated_value, value);
            }
            else {
                LOG_DEBUG("UVC_GET_CUR:ae res mode is invalid.");
            }
        }
        break;

    case UVC_SET_CUR:  // Translating from RS down to UVC 1.5 Spec
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            auto res = getCtrl(UVC_GET_RES, UVC_CT_AE_MODE_CONTROL, uvc_get_input_terminals(uvcDevHandle_)->bTerminalID);
            LOG_DEBUG("UVC_SET_CUR: getAERes:{}.", res);
            if(res & UVC_AE_MODE_D0_MANUAL) {
                auto autoValue   = (res & (~UVC_AE_MODE_D0_MANUAL));
                translated_value = value ? autoValue : UVC_AE_MODE_D0_MANUAL;
                LOG_DEBUG("UVC_AE_MODE_D0_MANUAL:translated_value:{},value:{}.", translated_value, value);
            }
            else if(res & UVC_AE_MODE_D1_AUTO) {
                auto manualValue = (res & (~UVC_AE_MODE_D1_AUTO));
                translated_value = value ? UVC_AE_MODE_D1_AUTO : manualValue;
                LOG_DEBUG("UVC_AE_MODE_D1_AUTO:translated_value:{},value:{}.", translated_value, value);
            }
            else {
                LOG_DEBUG("UVC_SET_CUR:ae res mode is invalid.");
            }
        }
        break;

    case UVC_GET_MIN:
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            translated_value = 0;  // Hardcoded MIN value
        }
        break;

    case UVC_GET_MAX:
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            translated_value = 1;  // Hardcoded MAX value
        }
        break;

    case UVC_GET_RES:
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            translated_value = 1;  // Hardcoded RES (step) value
        }
        break;

    case UVC_GET_DEF:
        if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL) {
            translated_value = 1;  // Hardcoded DEF value
        }
        break;

    default:
        throw std::runtime_error("Unsupported action translation");
    }
    return translated_value;
}

void ObLibuvcDevicePort::onFrameCallback(uvc_frame *frame, void *userPtr) {
    TRY_EXECUTE({
        OBUvcStreamHandle *handle     = (OBUvcStreamHandle *)userPtr;
        auto               rawframe   = FrameFactory::createFrameFromStreamProfile(handle->profile);
        auto               videoFrame = rawframe->as<VideoFrame>();

        videoFrame->updateData(static_cast<const uint8_t *>(frame->data), frame->data_bytes);

        auto payload_header_bytes = frame->payload_header_bytes > 12 ? 12 : frame->payload_header_bytes;
        videoFrame->updateMetadata(static_cast<const uint8_t *>(frame->payload_header), payload_header_bytes);
        auto metadata_bytes = frame->metadata_bytes > 255 ? 255 : frame->metadata_bytes;
        videoFrame->appendMetadata(static_cast<const uint8_t *>(frame->metadata), metadata_bytes);

        auto realtime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        videoFrame->setSystemTimeStampUsec(realtime);
        videoFrame->setTimeStampUsec(frame->pts);
        videoFrame->setNumber(frame->sequence);

        handle->callback(videoFrame);
    });
}

int32_t ObLibuvcDevicePort::getCtrl(uvc_req_code action, uint8_t control, uint8_t unit) const {
    unsigned char buffer[4] = { 0 };
    int32_t       ret       = 0;

    uint32_t transferred;

    transferred = uvc_get_ctrl(uvcDevHandle_, unit, control, buffer, sizeof(buffer), action);

    if(control == UVC_PU_BRIGHTNESS_CONTROL || control == UVC_PU_HUE_CONTROL) {
        ret = (short)SW_TO_SHORT(buffer);
        return ret;
    }

    switch(transferred) {
    case sizeof(uint8_t):
        ret = *(int32_t *)buffer;
        break;
    case sizeof(uint16_t):
        ret = SW_TO_SHORT(buffer);
        break;
    case sizeof(uint32_t):
        ret = DW_TO_INT(buffer);
        break;
    default:
        throw std::runtime_error("unsupported length");
    }

    return ret;
}

int ObLibuvcDevicePort::obPropToUvcCS(OBPropertyID propertyId, int &unit) const {

    unit = uvc_get_processing_units(uvcDevHandle_)->bUnitID;

    switch(propertyId) {
    case OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT:
        return UVC_PU_BACKLIGHT_COMPENSATION_CONTROL;
    case OB_PROP_COLOR_BRIGHTNESS_INT:
        return UVC_PU_BRIGHTNESS_CONTROL;
    case OB_PROP_COLOR_CONTRAST_INT:
        return UVC_PU_CONTRAST_CONTROL;
    case OB_PROP_COLOR_EXPOSURE_INT:
        unit = uvc_get_input_terminals(uvcDevHandle_)->bTerminalID;
        return UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL;
    case OB_PROP_COLOR_GAIN_INT:
        return UVC_PU_GAIN_CONTROL;
    case OB_PROP_COLOR_GAMMA_INT:
        return UVC_PU_GAMMA_CONTROL;
    case OB_PROP_COLOR_HUE_INT:
        return UVC_PU_HUE_CONTROL;
    case OB_PROP_COLOR_SATURATION_INT:
        return UVC_PU_SATURATION_CONTROL;
    case OB_PROP_COLOR_SHARPNESS_INT:
        return UVC_PU_SHARPNESS_CONTROL;
    case OB_PROP_COLOR_WHITE_BALANCE_INT:
        return UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL;
    case OB_PROP_COLOR_AUTO_EXPOSURE_BOOL:
        unit = uvc_get_input_terminals(uvcDevHandle_)->bTerminalID;
        return UVC_CT_AE_MODE_CONTROL;  // Automatic gain/exposure control
    case OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL:
        return UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL;
    case OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT:
        return UVC_PU_POWER_LINE_FREQUENCY_CONTROL;
    case OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT:
        unit = uvc_get_input_terminals(uvcDevHandle_)->bTerminalID;
        return UVC_CT_AE_PRIORITY_CONTROL;
    case OB_PROP_COLOR_ROLL_INT:
        unit = uvc_get_input_terminals(uvcDevHandle_)->bTerminalID;
        return UVC_CT_ROLL_ABSOLUTE_CONTROL;
    case OB_PROP_COLOR_FOCUS_INT:
        unit = uvc_get_input_terminals(uvcDevHandle_)->bTerminalID;
        return UVC_CT_FOCUS_ABSOLUTE_CONTROL;
    default:
        throw pal_exception(utils::string::to_string() << "invalid propertyId : " << propertyId);
    }
}

void ObLibuvcDevicePort::setCtrl(uvc_req_code action, uint8_t control, uint8_t unit, int32_t value) const {
    (void)action;
    unsigned char buffer[4];
    INT_TO_DW(value, buffer);

    uint32_t transferred;
    transferred = uvc_set_ctrl(uvcDevHandle_, unit, control, buffer, sizeof(int32_t));
    (void)transferred;
}

std::vector<ObLibuvcDevicePort::uvcProfile> ObLibuvcDevicePort::queryAvailableUvcProfile() const {
    std::vector<uvcProfile> rv;

    uvc_streaming_interface_t *stream_if;
    DL_FOREACH(uvcDevHandle_->info->stream_ifs, stream_if) {
        uvc_format_desc_t *fmt_desc;
        // LOG(INFO)<<"stream_if.interfaceNumber ="<<(int)(stream_if->bInterfaceNumber);

        DL_FOREACH(stream_if->format_descs, fmt_desc) {
            uvc_frame_desc_t *frame_desc;
            switch(fmt_desc->bDescriptorSubtype) {
            case UVC_VS_FORMAT_UNCOMPRESSED:
            case UVC_VS_FORMAT_MJPEG:
            case UVC_VS_FORMAT_FRAME_BASED:

                DL_FOREACH(fmt_desc->frame_descs, frame_desc) {
                    uint32_t *interval_ptr;

                    uvcProfile uvcFormat{};
                    uvcFormat.width  = (uint32_t)frame_desc->wWidth;
                    uvcFormat.height = (uint32_t)frame_desc->wHeight;
                    uvcFormat.fourcc = fourCc2Int(fmt_desc->fourccFormat[0], fmt_desc->fourccFormat[1], fmt_desc->fourccFormat[2], fmt_desc->fourccFormat[3]);

                    uvcFormat.interfaceNumber = stream_if->bInterfaceNumber;
                    uvcFormat.endpointAddress = stream_if->bEndpointAddress;
                    if(frame_desc->intervals) {
                        for(interval_ptr = frame_desc->intervals; *interval_ptr; ++interval_ptr) {
                            uvcFormat.fps = 10000000 / *interval_ptr;
                            rv.push_back(uvcFormat);
                        }
                    }
                }
            default:
                break;
            }
        }
    }

    return rv;
}

std::shared_ptr<const SourcePortInfo> ObLibuvcDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

#ifdef __ANDROID__
std::string ObLibuvcDevicePort::getUsbConnectType() {
    libusb_device_descriptor desc;
    auto                     ret = libusb_get_device_descriptor(uvcDev_->usb_dev, &desc);
    return usb_spec_names.find(UsbSpec(desc.bcdUSB))->second;
}
#endif

#ifdef __linux__
OBUvcBackendType ObLibuvcDevicePort::getBackendType() const {
    return OB_UVC_BACKEND_TYPE_LIBUVC;
}
#endif

}  // namespace libobsensor
