// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

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

    int imuRetryReadNum;
    int imuReadFps;
    int imuPollInterval;
};

}  // namespace libobsensor
