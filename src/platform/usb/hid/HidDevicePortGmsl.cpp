// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HidDevicePortGmsl.hpp"
#include "usb/uvc/ObV4lGmslHostProtocolTypes.hpp"

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "frame/FrameFactory.hpp"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/usb/video.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <random>

namespace libobsensor {

const int IMU_FRAME_MAX_NUM  = 9;  // 5;
const int IMU_RETRY_READ_NUM = 30;
const int POLL_INTERVAL      = 10;  // poll interval time

/**
 * cal: 1*1000/fps * (IMU_FRAME_MAX_NUM-4)
 */
int calculateImuPollInterval(int mImuReadFps) {
    int delayTime = 0;
    delayTime     = (1000 / mImuReadFps) * (IMU_FRAME_MAX_NUM - 4);
    LOG_DEBUG("calculateImuPollInterval:{}, mImuReadFps:{}", delayTime, mImuReadFps);
    if(delayTime < 5)
        delayTime = 5;

    // LOG_DEBUG("-calculateImuPollInterval-2: {} ", delayTime );
    return delayTime;
}

/**
 * 500HZ
 * 1*1000/fps * (IMU_FRAME_MAX_NUM-1) -10ms
 */
int calculateImuPollInterval500HZ(int mImuReadFps) {
    int delayTime = 0;
    delayTime     = (1000 / mImuReadFps) * (IMU_FRAME_MAX_NUM - 1) - 10;
    LOG_DEBUG("calculateImuPollInterval500HZ:{}, mImuReadFps:{}", delayTime, mImuReadFps);
    if(delayTime < 5)
        delayTime = 5;

    // LOG_DEBUG("-calculateImuPollInterval-2: {} ", delayTime );
    return delayTime;
}

void imuSleepMs(int mSleepMs) {
    // utils::sleepMs(mSleepMs);
    std::this_thread::sleep_for(std::chrono::milliseconds(mSleepMs));
}

HidDevicePortGmsl::HidDevicePortGmsl(std::shared_ptr<const USBSourcePortInfo> portInfo) : portInfo_(portInfo), isStreaming_(false), frameQueue_(10) {
    imu_fd_ = open(portInfo_->infName.c_str(), O_RDWR);
    if(imu_fd_ < 0) {
        throw pal_exception(utils::string::to_string() << "HidDevicePortGmsl() openDev failed, errno: " << errno << " " << strerror(errno));
    }
    LOG_DEBUG("HidDevicePortGmsl() imu_fd_:{}", imu_fd_);
}

HidDevicePortGmsl::~HidDevicePortGmsl() noexcept {
    LOG_DEBUG("~HidDevicePortGmsl()");
    stopStream();
    if(imu_fd_ >= 0) {
        close(imu_fd_);
    }
    imuSleepMs(100);  // sleep 100ms to wait backend release resource
    LOG_DEBUG("HidDevicePortGmsl destroyed");
}

std::shared_ptr<const SourcePortInfo> HidDevicePortGmsl::getSourcePortInfo() const {
    return portInfo_;
}

void HidDevicePortGmsl::startStream(MutableFrameCallback callback) {
    LOG_DEBUG("Try to start stream");
    imuRetryReadNum = IMU_RETRY_READ_NUM;  // default 10
    imuReadFps      = 200;                 // default 200Hz
    imuPollInterval = 25;                  // default 25ms

    if(isStreaming_) {
        throw wrong_api_call_sequence_exception("HidDevicePort::startStream() called while streaming");
    }
    isStreaming_ = true;
    frameQueue_.start(callback);

    // init poll thread
    pollThread_ = std::thread([this]() {
        while(true) {
            if(!isStreaming_) {
                break;
            }
            if(imuRetryReadNum > 0) {
                imuReadFps      = getImuFps();
                imuPollInterval = (imuReadFps >= 500) ? 5 : calculateImuPollInterval(imuReadFps);
                LOG_DEBUG("get current imuReadFps:{}, imuPollInterval:{}ms, imuRetryReadNum:{} ", imuReadFps.load(), imuPollInterval.load(),
                          imuRetryReadNum.load());
                imuRetryReadNum--;
            }
            imuSleepMs(imuPollInterval);
            pollData();
        }
    });
    LOG_DEBUG("HidDevicePortGmsl::startCapture done");
}

void HidDevicePortGmsl::stopStream() {
    if(!isStreaming_) {
        LOG_WARN("stopStream() called while not streaming");
        return;
    }
    LOG_DEBUG("stopCapture start");
    isStreaming_ = false;
    if(pollThread_.joinable()) {
        pollThread_.join();
    }
    LOG_DEBUG("release frameQueue");
    frameQueue_.flush();
    LOG_DEBUG("frameQueue_.flush");
    frameQueue_.reset();
    LOG_DEBUG("stopCapture done");
}

// Original imu data, software packaging method, needs to be calculated on the sdk side
typedef struct {
    uint8_t  reportId;    // Firmware fixed transmission 1
    uint8_t  sampleRate;  // OB_SAMPLE_RATE
    uint8_t  groupLen;    // sizeof(OBImuOriginData)
    uint8_t  groupCount;  // How many frames of data are in a packet.
    uint32_t reserved;    // reserve
} OBImuHeader;

typedef struct {
    int16_t  groupId;  // The number of groups in a pack
    int16_t  accelX;
    int16_t  accelY;
    int16_t  accelZ;
    int16_t  gyroX;
    int16_t  gyroY;
    int16_t  gyroZ;
    int16_t  temperature;
    uint32_t timestamp[2];
} OBImuOriginData;

typedef struct {
    OBImuHeader     imuHeader;                        // imuHeader
    OBImuOriginData imuFrameData[IMU_FRAME_MAX_NUM];  // OBImuOriginData
    uint32_t        reserved;                         // Reserved
} OBImuOrigMsg;

void HidDevicePortGmsl::pollData() {
    int groupCount = 0;
    // LOG_DEBUG("HidDevicePortGmsl::pollData start...");
    auto frame = FrameFactory::createFrame(OB_FRAME_UNKNOWN, OB_FORMAT_UNKNOWN, sizeof(OBImuOrigMsg));

    OBImuOrigMsg *imuOrigFrameMsg = reinterpret_cast<OBImuOrigMsg *>(frame->getDataMutable());
    memset(imuOrigFrameMsg, 0, sizeof(OBImuOrigMsg));
    imuOrigFrameMsg->imuHeader.reportId   = 1;
    imuOrigFrameMsg->imuHeader.sampleRate = 200;
    imuOrigFrameMsg->imuHeader.groupLen   = sizeof(OBImuOriginData);
    imuOrigFrameMsg->imuHeader.groupCount = 1;
    imuOrigFrameMsg->imuHeader.reserved   = 0;
    imuOrigFrameMsg->reserved             = 0;

    uint8_t imuFrameMaxSize = sizeof(OBImuOriginData) * (IMU_FRAME_MAX_NUM) + sizeof(i2c_msg_header_t) + sizeof(uint16_t);
    auto                 bufferSize      = std::max(sizeof(i2c_msg_t), static_cast<size_t>(imuFrameMaxSize));
    std::vector<uint8_t> databuf(bufferSize, 0);
    int                  size = getImuData(databuf.data());

    if(size < 0) {
        LOG_ERROR("read getImuData failed! ret:{} \n", size);
        return;
    }

    auto *mOriginI2CMsg = reinterpret_cast<i2c_msg_t *>(&databuf[0]);
    int   mOnceReadLen  = mOriginI2CMsg->header.len;
    // LOG_DEBUG("-read imu data header.len: {}", mOnceReadLen );

    if((mOriginI2CMsg->body.res == 0x00) && (mOnceReadLen > 8)) {
        groupCount = (mOnceReadLen - 8) / sizeof(OBImuOriginData);
        // LOG_DEBUG("-read imu data groupCount:{}, header.len:{}, imuPollInterval:{}ms ", groupCount, mOnceReadLen, imuPollInterval);

        if(groupCount > 0) {
            imuOrigFrameMsg->imuHeader.groupCount = groupCount;
            int imuOriginHeaderOffset             = sizeof(i2c_msg_header_t) + 2 /*res*/;

            auto *tmp = reinterpret_cast<imu_origin_data_t *>(&databuf[imuOriginHeaderOffset]);

#if 0  // for test debug
            LOG_DEBUG("-> group_id: {}  ", tmp->group_id);
            LOG_DEBUG("-> accel_x:  {}  ", tmp->accel_x);
            LOG_DEBUG("-> accel_y:  {}  ", tmp->accel_y);
            LOG_DEBUG("-> accel_z:  {}  ", tmp->accel_z);
            LOG_DEBUG("-> gyro_x:   {}  ", tmp->gyro_x);
            LOG_DEBUG("-> gyro_y:   {}  ", tmp->gyro_y);
            LOG_DEBUG("-> gyro_z:   {}  ", tmp->gyro_z);
            LOG_DEBUG("-> temp:     {}  ", tmp->temp);
            LOG_DEBUG("-> timestamp[0]: {}  ", tmp->timestamp[0]);
            LOG_DEBUG("-> timestamp[1]: {}  \n", tmp->timestamp[1]);
#endif

            memcpy(imuOrigFrameMsg->imuFrameData, tmp, sizeof(imu_origin_data_t) * groupCount);

#if 0  // for test debug
            for(int i=0; i< groupCount; i++) {
                LOG_DEBUG("->{} groupId: {}  ", i, imuOrigFrameMsg.imuFrameData[i].groupId);
                LOG_DEBUG("->{} accelX:  {}  ", i, imuOrigFrameMsg.imuFrameData[i].accelX);
                LOG_DEBUG("->{} accelY:  {}  ", i, imuOrigFrameMsg.imuFrameData[i].accelY);
                LOG_DEBUG("->{} accelZ:  {}  ", i, imuOrigFrameMsg.imuFrameData[i].accelZ);
                LOG_DEBUG("->{} gyroX:   {}  ", i, imuOrigFrameMsg.imuFrameData[i].gyroX);
                LOG_DEBUG("->{} gyroY:   {}  ", i, imuOrigFrameMsg.imuFrameData[i].gyroY);
                LOG_DEBUG("->{} gyroZ:   {}  ", i, imuOrigFrameMsg.imuFrameData[i].gyroZ);
                LOG_DEBUG("->{} temperature:  {}  ", i, imuOrigFrameMsg.imuFrameData[i].temperature);
                LOG_DEBUG("->{} timestamp[0]: {}  ", i, imuOrigFrameMsg.imuFrameData[i].timestamp[0]);
                LOG_DEBUG("->{} timestamp[1]: {}  \n", i, imuOrigFrameMsg.imuFrameData[i].timestamp[1]);
            }
#endif
            auto realtime = utils::getNowTimesUs();
            frame->setSystemTimeStampUsec(realtime);
            frameQueue_.enqueue(frame);
        }
        else {
            LOG_DEBUG("read imu data groupCount is 0 ");
        }
    }
}

int HidDevicePortGmsl::getImuFps() {
    int      ret  = 0;
    uint32_t size = 0;

    v4l2_ext_controls ctrls;
    v4l2_ext_control  ctrl;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ctrls.controls   = &ctrl;
    ctrls.count      = 1;

    ctrl.id    = ORBBEC_CAMERA_CID_GET_IMU_FPS;
    ctrl.size  = 4;
    ctrl.p_u32 = &size;
    {
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        ret = xioctlGmsl(imu_fd_, VIDIOC_G_EXT_CTRLS, &ctrls);
    }
    if(ret < 0) {
        LOG_ERROR("{}:{} ioctl failed on getdate errno:{}, strerror:{} \n ", __FILE__, __LINE__, errno, strerror(errno));
        return -1;
    }

    if((size == 0) || (size < 50)) {
        size = 200;  // default 200HZ
    }

    return size;
}

int HidDevicePortGmsl::getImuData(uint8_t *data) {
    int               ret = 0;
    v4l2_ext_controls ctrls;
    v4l2_ext_control  ctrl;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ctrls.controls   = &ctrl;
    ctrls.count      = 1;

    ctrl.id   = G2R_CAMERA_CID_GET_IMU_DATA;
    int size  = sizeof(OBImuOriginData) * (IMU_FRAME_MAX_NUM) + sizeof(i2c_msg_header_t) + sizeof(uint16_t);
    ctrl.size = size;
    ctrl.p_u8 = data;

    {
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        ret = xioctlGmsl(imu_fd_, VIDIOC_G_EXT_CTRLS, &ctrls);
    }

    if(ret < 0) {
        LOG_ERROR("{}:{} ioctl failed on getdate :{}\n ", __FILE__, __LINE__, strerror(errno));
        return -1;
    }
    return 0;
}

}  // namespace libobsensor

