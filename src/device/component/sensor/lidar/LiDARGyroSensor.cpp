// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARGyroSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

LiDARGyroSensor::LiDARGyroSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_GYRO, backend), streamer_(streamer) {
    LOG_DEBUG("LiDARGyroSensor is created!");
}

LiDARGyroSensor::~LiDARGyroSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("LiDARGyroSensor is destroyed");
}

void LiDARGyroSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    // validate device state
    validateDeviceState(sp);

    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    BEGIN_TRY_EXECUTE({
        streamer_->startStream(sp, [this](std::shared_ptr<Frame> frame) {
            if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
                return;
            }

            updateStreamState(STREAM_STATE_STREAMING);
            outputFrame(frame);
        });
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        throw;
    })
}

void LiDARGyroSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stopStream(activatedStreamProfile_);
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor
