// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameQueue.hpp"

#include <cstdio>
#include <cstdlib>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <set>

namespace libobsensor {

class HidDevicePortGmsl : public IDataStreamPort {
public:
    HidDevicePortGmsl(std::shared_ptr<const USBSourcePortInfo> portInfo);
    virtual ~HidDevicePortGmsl() noexcept;

    void startStream(MutableFrameCallback callback) override;
    void stopStream() override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    void pollData();
    int  getImuData(uint8_t *data);
    int  getImuFps();

private:
    int imu_fd_;

    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    std::atomic_bool                         isStreaming_;
    MutableFrameCallback                     frameCallback_;
    FrameQueue<Frame>                        frameQueue_;

    std::thread pollThread_;

    std::atomic_int imuRetryReadNum;
    std::atomic_int imuReadFps;
    std::atomic_int imuPollInterval;
};

}  // namespace libobsensor

