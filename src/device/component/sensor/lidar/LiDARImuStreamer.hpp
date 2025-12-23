// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFilter.hpp"
#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include "IStreamer.hpp"
#include "stream/StreamProfile.hpp"
#include "ethernet/LiDARDataStreamPort.hpp"

#include <atomic>
#include <map>
#include <mutex>

namespace libobsensor {

class LiDARImuStreamer : public IDeviceComponent, public IStreamer {
public:
    LiDARImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    virtual ~LiDARImuStreamer() noexcept override;

    virtual void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void stopStream(std::shared_ptr<const StreamProfile> profile) override;
    IDevice     *getOwner() const override;

private:
    void         trySendStartStreamVendorCmd();
    void         trySendStopStreamVendorCmd();
    virtual void parseIMUData(std::shared_ptr<Frame> frame);
    virtual void outputFrame(std::shared_ptr<Frame> accelFrame, std::shared_ptr<Frame> gyroFrame);

    void convertAccelUnit(std::shared_ptr<Frame> frame);
    void convertGyroUnit(std::shared_ptr<Frame> frame);

private:
    IDevice                              *owner_;
    std::shared_ptr<LiDARDataStreamPort>  backend_;

    std::mutex                                cbMtx_;
    MutableFrameCallback                      accelCallback_      = nullptr;
    MutableFrameCallback                      gyroCallback_       = nullptr;
    std::shared_ptr<const AccelStreamProfile> accelStreamProfile_ = nullptr;
    std::shared_ptr<const GyroStreamProfile>  gyroStreamProfile_  = nullptr;

    std::atomic_bool running_;
    uint64_t         frameIndex_;

    int firmwareVersionInt_;
};
}  // namespace libobsensor
