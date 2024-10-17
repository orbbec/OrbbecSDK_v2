// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <array>
#include <string>
#include <thread>
#include <mutex>
#include <sys/mman.h>

#include "UvcDevicePort.hpp"
#include "stream/StreamProfile.hpp"

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/usb/video.h>

#ifdef V4L2_META_FMT_UVC
constexpr bool metadata_node = true;
#else

constexpr bool metadata_node = false;

// Providing missing parts from videodev2.h
// V4L2_META_FMT_UVC >> V4L2_CAP_META_CAPTURE is also defined, but the opposite does not hold
#define V4L2_META_FMT_UVC v4l2_fourcc('U', 'V', 'C', 'H') /* UVC Payload Header */

#ifndef V4L2_CAP_META_CAPTURE
#define V4L2_CAP_META_CAPTURE 0x00800000 /* Specified in kernel header v4.16 */
#endif                                   // V4L2_CAP_META_CAPTURE

#endif  // V4L2_META_FMT_UVC

namespace libobsensor {

static const uint32_t MAX_META_DATA_SIZE       = 255;
static const uint32_t MAX_BUFFER_COUNT         = 4;
static const uint32_t LOCAL_V4L2_META_FMT_D4XX = v4l2_fourcc('D', '4', 'X', 'X');  // borrows from videodev2.h, using for getting extention metadata
#define LOCAL_V4L2_BUF_TYPE_META_CAPTURE ((v4l2_buf_type)13)

#pragma pack(push, 1)
struct V4L2UvcMetaHeader {
    uint64_t ns;   // system timestamp of the payload in nanoseconds
    uint16_t sof;  // USB Frame Number
    // the rest of the meta buffer is the UVC Payload Header
};
#pragma pack(pop)

struct V4L2FrameBuffer {
    ~V4L2FrameBuffer() {
        if(ptr != nullptr) {
            munmap(ptr, length);
            ptr = nullptr;
        }
    }
    uint32_t length        = 0;
    uint32_t actual_length = 0;
    uint32_t sequence      = 0;
    uint8_t *ptr           = nullptr;
};

struct V4lDeviceInfo {
    std::string     name;
    v4l2_capability cap;  // capabilities
};

struct V4lDeviceHandle {
    std::shared_ptr<V4lDeviceInfo>                info;
    int                                           fd = -1;
    std::array<V4L2FrameBuffer, MAX_BUFFER_COUNT> buffers;

    std::shared_ptr<V4lDeviceInfo>                metadataInfo;
    int                                           metadataFd = -1;
    std::array<V4L2FrameBuffer, MAX_BUFFER_COUNT> metadataBuffers;

    MutableFrameCallback                      frameCallback;
    std::shared_ptr<const VideoStreamProfile> profile = nullptr;

    int                          stopPipeFd[2] = { -1, -1 };  // pipe to signal the capture thread to stop
    std::shared_ptr<std::thread> captureThread = nullptr;
    std::atomic<bool>            isCapturing   = { false };
};

class ObV4lUvcDevicePort : public UvcDevicePort {
public:
    explicit ObV4lUvcDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~ObV4lUvcDevicePort() noexcept override;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

    StreamProfileList getStreamProfileList() override;

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void stopAllStream() override;

    bool            getPu(uint32_t propertyId, int32_t &value) override;
    bool            setPu(uint32_t propertyId, int32_t value) override;
    UvcControlRange getPuRange(uint32_t propertyId) override;
    uint32_t        sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) override;

    static std::vector<std::shared_ptr<V4lDeviceInfo>> queryRelatedDevices(std::shared_ptr<const USBSourcePortInfo> portInfo);
    static bool                                        isContainedMetadataDevice(std::shared_ptr<const USBSourcePortInfo> portInfo);

private:
    static void     captureLoop(std::shared_ptr<V4lDeviceHandle> deviceHandle);
    bool            getXu(uint8_t ctrl, uint8_t *data, uint32_t *len);
    bool            setXu(uint8_t ctrl, const uint8_t *data, uint32_t len);
    UvcControlRange getXuRange(uint8_t control, int len);
    bool            pendForCtrlStatusEvent() const;
    void            subscribeToCtrlEvent(uint32_t ctrl_id) const;
    void            unsubscribeFromCtrlEvent(uint32_t ctrl_id) const;

private:
    std::shared_ptr<const USBSourcePortInfo>      portInfo_ = nullptr;
    std::vector<std::shared_ptr<V4lDeviceHandle>> deviceHandles_;
    std::recursive_mutex                          ctrlMutex_;
};

}  // namespace libobsensor

