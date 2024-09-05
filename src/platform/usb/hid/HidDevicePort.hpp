// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#pragma once
#include "ISourcePort.hpp"
#include "frame/FrameQueue.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

namespace libobsensor {

class HidDevicePort : public IDataStreamPort {
public:
    HidDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~HidDevicePort() noexcept override;

    void startStream(MutableFrameCallback callback) override;
    void stopStream() override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    std::shared_ptr<IUsbDevice>              usbDevice_;
    uint8_t                                  endpointAddress_;
    uint16_t                                 maxPacketSize_;

    std::atomic_bool  isStreaming_;
    FrameQueue<Frame> frameQueue_;

    std::thread streamThread_;
};

}  // namespace libobsensor
