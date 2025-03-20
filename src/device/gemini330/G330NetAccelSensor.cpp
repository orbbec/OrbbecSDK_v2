// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330NetAccelSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "ethernet/RTPStreamPort.hpp"

namespace libobsensor {


G330NetAccelSensor::G330NetAccelSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<ImuStreamer> &streamer) 
    : AccelSensor(owner, backend, streamer) {}

G330NetAccelSensor::~G330NetAccelSensor() noexcept {
}

void G330NetAccelSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto     rtpStreamPort = std::dynamic_pointer_cast<RTPStreamPort>(backend_);
    uint16_t port          = rtpStreamPort->getStreamPort();
    LOG_DEBUG("IMU start stream port: {}", port);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    propServer->setPropertyValueT(OB_PROP_IMU_STREAM_PORT_INT, static_cast<int>(port));
    AccelSensor::start(sp, callback);
}

void G330NetAccelSensor::stop() {
    AccelSensor::stop();
}

}  // namespace libobsensor
