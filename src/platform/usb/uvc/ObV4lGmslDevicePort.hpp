// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <array>
#include <string>
#include <thread>
#include <sys/mman.h>

#include "UvcDevicePort.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"
#include "frame/Frame.hpp"

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/usb/video.h>

#if 0

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

#endif

namespace libobsensor {

static const uint16_t GMSL_VID_ORBBEC = 0x2BC5;
static const uint16_t GMSL_PID_G335L  = 0x080B;  // 0x06D0;  // D457 dev

static const std::string GMSL_SN_DEFAULT      = "0123456789";
static const std::string GMSL_ASIC_SN_DEFAULT = "12345";

static const uint32_t MAX_META_DATA_SIZE_GMSL = 255;
static const uint32_t MAX_BUFFER_COUNT_GMSL   = 8;  // 4; //v4l3_buf default 4 buf. opt to 4->8
// static const uint32_t LOCAL_V4L2_META_FMT_D4XX_GMSL = v4l2_fourcc('D', '4', 'X', 'X'); // borrows from videodev2.h, using for getting extention metadata
static const uint32_t LOCAL_V4L2_META_FMT_D4XX_GMSL = v4l2_fourcc('G', '2', 'X', 'X');  // borrows from videodev2.h, using for getting extention metadata

#define LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL ((v4l2_buf_type)13)

// #define OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX (256-16) //248 //246 //128 //160 //172 //192 //208 //224 //240 //246 //252  //fw max_read_len is 252
// #define OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX   //246 //128 //160 //172 //192 //208 //224 //240 //246 //252  //fw max_read_len is 252

typedef enum {
    OB_GMSL_FW_I2C_DATA_LEN_16  = 16,
    OB_GMSL_FW_I2C_DATA_LEN_32  = 32,
    OB_GMSL_FW_I2C_DATA_LEN_64  = 64,
    OB_GMSL_FW_I2C_DATA_LEN_128 = 128,
    OB_GMSL_FW_I2C_DATA_LEN_192 = 192,
    OB_GMSL_FW_I2C_DATA_LEN_256 = 256,
    OB_GMSL_FW_I2C_DATA_LEN_512 = 512,
} ObGmslFwI2CDataLen;

typedef enum {
    OB_GMSL_GET_FIRMWARE_DATA_CTRL_ID = 1,
} ObGmslGetFwDataCtrId;

#pragma pack(push, 1)
struct V4L2UvcMetaHeaderGmsl {
    uint64_t ns;   // system timestamp of the payload in nanoseconds
    uint16_t sof;  // USB Frame Number
    // the rest of the meta buffer is the UVC Payload Header
};
#pragma pack(pop)

struct V4L2FrameBufferGmsl {
    ~V4L2FrameBufferGmsl() {
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

struct V4lDeviceInfoGmsl {
    std::string     name;
    v4l2_capability cap;  // capabilities
};

struct V4lDeviceHandleGmsl {
    std::shared_ptr<V4lDeviceInfoGmsl>                     info;
    int                                                    fd;
    std::array<V4L2FrameBufferGmsl, MAX_BUFFER_COUNT_GMSL> buffers;

    std::shared_ptr<V4lDeviceInfoGmsl>                     metadataInfo;
    int                                                    metadataFd;
    std::array<V4L2FrameBufferGmsl, MAX_BUFFER_COUNT_GMSL> metadataBuffers;

    MutableFrameCallback                      frameCallback;
    std::shared_ptr<const VideoStreamProfile> profile = nullptr;

    int                          stopPipeFd[2] = { -1, -1 };  // pipe to signal the capture thread to stop
    std::shared_ptr<std::thread> captureThread = nullptr;
    std::atomic<bool>            isCapturing   = { false };
    std::atomic<std::uint64_t>   loopFrameIndex = { 0 };
};

class ObV4lGmslDevicePort : public UvcDevicePort {
public:
    explicit ObV4lGmslDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~ObV4lGmslDevicePort() noexcept override;

    StreamProfileList getStreamProfileList() override;

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void stopAllStream() override;

    uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) override;

    bool            getPu(uint32_t propertyId, int32_t &value) override;
    bool            setPu(uint32_t propertyId, int32_t value) override;
    UvcControlRange getPuRange(uint32_t propertyId) override;

    virtual std::shared_ptr<const SourcePortInfo>          getSourcePortInfo() const override;
    static std::vector<std::shared_ptr<V4lDeviceInfoGmsl>> queryRelatedDevices(std::shared_ptr<const USBSourcePortInfo> portInfo);
    static bool                                            isContainedMetadataDevice(std::shared_ptr<const USBSourcePortInfo> portInfo);
    static bool                                            isGmslDeviceForPlatformNvidia(std::shared_ptr<const USBSourcePortInfo> portInfo);

    static const std::vector<UsbInterfaceInfo> queryDevicesInfo();

protected:
    int resetGmslDriver();

private:
    static void captureLoop(std::shared_ptr<V4lDeviceHandleGmsl> deviceHandle);

    bool setPuRaw(uint32_t propertyId, int32_t value);

    bool sendData(const uint8_t *data, uint32_t dataLen);
    bool recvData(uint8_t *data, uint32_t *dataLen);
    bool getXuExt(uint32_t ctrl, uint8_t *data, uint32_t *len);
    bool setXuExt(uint32_t ctrl, const uint8_t *data, uint32_t len);

    static void handleSpecialResolution(std::shared_ptr<V4lDeviceHandleGmsl> devHandle, const uint8_t *srcData, uint32_t srcSize,
                                        std::shared_ptr<VideoFrame> videoFrame);

private:
    std::shared_ptr<const USBSourcePortInfo>          portInfo_ = nullptr;
    std::vector<std::shared_ptr<V4lDeviceHandleGmsl>> deviceHandles_;
};

}  // namespace libobsensor

