// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330NetGyroSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "ethernet/RTPStreamPort.hpp"

namespace libobsensor {

G330NetGyroSensor::G330NetGyroSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<ImuStreamer> &streamer)
    : GyroSensor(owner, backend, streamer) {
    stopStreamByVendorCmd();
}

G330NetGyroSensor::~G330NetGyroSensor() noexcept {
}

void G330NetGyroSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    auto     rtpStreamPort = std::dynamic_pointer_cast<RTPStreamPort>(backend_);
    uint16_t port          = rtpStreamPort->getStreamPort();
    LOG_DEBUG("IMU start stream port: {}", port);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    propServer->setPropertyValueT(OB_PROP_IMU_STREAM_PORT_INT, static_cast<int>(port));
    GyroSensor::start(sp, callback);
}

void G330NetGyroSensor::stop() {
    GyroSensor::stop();
}

void G330NetGyroSensor::stopStreamByVendorCmd() {
    auto propServer = getOwner()->getPropertyServer();
    BEGIN_TRY_EXECUTE({ propServer->setPropertyValueT(OB_PROP_GYRO_SWITCH_BOOL, false); })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to send stop command for gyro stream"); })
}

}  // namespace libobsensor
