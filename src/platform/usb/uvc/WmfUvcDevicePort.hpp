// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "UvcDevicePort.hpp"
#include "stream/StreamProfile.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

#include "WinHelpers.hpp"

#include <mfapi.h>
#include <Ks.h>
#include <atlcomcli.h>
#include <ksproxy.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vidcap.h>
#include <wrl.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <mutex>
#include <atomic>

static const std::vector<std::vector<std::pair<GUID, GUID>>> attributes_params = {
    { { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID },
      { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, KSCATEGORY_SENSOR_CAMERA } },
    { { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID } },
};

namespace libobsensor {

class WinUsbPal;

struct StreamObject {
    bool                                      streaming = false;
    manual_reset_event                        isFlushed;
    manual_reset_event                        hasStarted;
    std::shared_ptr<const VideoStreamProfile> profile = nullptr;
    uint32_t                                  frameCounter;
    MutableFrameCallback                      callback = nullptr;
};

typedef std::function<void(const UsbInterfaceInfo &, IMFActivate *)> USBDeviceInfoEnumCallback;

typedef struct FrameRate {
    unsigned int denominator;
    unsigned int numerator;
} FrameRate;

typedef struct MFProfile {
    uint32_t                            index;
    FrameRate                           min_rate;
    FrameRate                           max_rate;
    std::shared_ptr<VideoStreamProfile> profile;
} MFProfile;

template <class T> static void safe_release(T &ppT) {
    if(ppT) {
        ppT.Release();
        ppT = NULL;
    }
}

class WmfUvcDevicePort : public std::enable_shared_from_this<WmfUvcDevicePort>,
                         public UvcDevicePort,
                         public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IMFSourceReaderCallback> {
public:
    explicit WmfUvcDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~WmfUvcDevicePort() noexcept override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo(void) const override;
    StreamProfileList                     getStreamProfileList() override;
    void                                  startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void                                  stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void                                  stopAllStream() override;

    bool            getPu(uint32_t propertyId, int32_t &value) override;
    bool            setPu(uint32_t propertyId, int32_t value) override;
    UvcControlRange getPuRange(uint32_t propertyId) override;

    uint32_t sendAndReceive(const uint8_t* sendData, uint32_t sendLen, uint8_t* recvData, uint32_t exceptedRecvLen) override;

    static bool isConnected(std::shared_ptr<const USBSourcePortInfo> info);
    static void foreachUvcDevice(const USBDeviceInfoEnumCallback &action);

private:
    IAMVideoProcAmp  *getVideoProc() const;
    IAMCameraControl *getCameraControl() const;

    void initXu();
    bool getXu(uint8_t ctrl, uint8_t *data, uint32_t *len);
    bool setXu(uint8_t ctrl, const uint8_t *data, uint32_t len);

private:
    friend class SourceReaderCallback;

    void                   playProfile(std::shared_ptr<const VideoStreamProfile> profile, MutableFrameCallback callback);
    void                   flush(int index);
    void                   checkConnection() const;
    CComPtr<IMFAttributes> createDeviceAttrs();
    CComPtr<IMFAttributes> createReaderAttrs();
    void                   foreachProfile(std::function<void(const MFProfile &profile, CComPtr<IMFMediaType> media_type, bool &quit)> action) const;

    void setPowerStateD0();  // create device source and source reader
    void setPowerStateD3();  // release device source and source reader
    void reloadSourceAndReader();

    void queryXuNodeId(const CComPtr<IKsTopologyInfo> &topologyInfo);

private:
    std::recursive_mutex deviceMutex_;

    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    PowerState                               powerState_ = kD3;

    CComPtr<IMFMediaSource>                 deviceSource_ = nullptr;
    CComPtr<IMFAttributes>                  deviceAttrs_  = nullptr;
    Microsoft::WRL::ComPtr<IMFSourceReader> streamReader_;
    CComPtr<IMFAttributes>                  readerAttrs_ = nullptr;

    CComPtr<IAMCameraControl> cameraControl_ = nullptr;
    CComPtr<IAMVideoProcAmp>  videoProc_     = nullptr;
    CComPtr<IKsControl>       xuKsControl_;
    int                       xuNodeId_;

    HRESULT readSampleResult_ = S_OK;

    std::map<uint32_t, StreamObject> streams_;  // <index, StreamObj>
    std::mutex                       streamsMutex_;

    std::atomic<bool> isStarted_ = { false };
    std::wstring      deviceId_;

    StreamProfileList profileList_;

public:
    STDMETHODIMP QueryInterface(REFIID iid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP OnReadSample(HRESULT /*hrStatus*/, DWORD dwStreamIndex, DWORD /*dwStreamFlags*/, LONGLONG /*llTimestamp*/, IMFSample *sample) override;
    STDMETHODIMP OnEvent(DWORD /*sidx*/, IMFMediaEvent * /*event*/) override;
    STDMETHODIMP OnFlush(DWORD) override;

private:
    long refCount_ = 1;
};

}  // namespace libobsensor

