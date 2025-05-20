// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

LiDARSensor::LiDARSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_LIDAR, backend), streamer_(streamer) {
    auto port = std::dynamic_pointer_cast<IDataStreamPort>(backend_);
    if(!port) {
        throw invalid_value_exception("Backend is not a valid IDataStreamPort");
    }
    LOG_DEBUG("LiDARSensor created @{}", sensorType_);
}

LiDARSensor::~LiDARSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("LiDARSensor destroyed @{}", sensorType_);
}

void LiDARSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    if ( isStreamActivated()) {
        throw unsupported_operation_exception(utils::string::to_string() << "The LiDAR stream has already been started.");
    }

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

void LiDARSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stopStream(activatedStreamProfile_);
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor
