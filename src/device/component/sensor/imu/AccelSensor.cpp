// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AccelSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

#pragma pack(1)
typedef struct {
    uint32_t          num;
    OBAccelSampleRate odr[16];
} AccelSampleRateList;

typedef struct {
    uint32_t              num;
    OBAccelFullScaleRange fs[16];
} AccelFullScaleRangeList;
#pragma pack()

AccelSensor::AccelSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<ImuStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_ACCEL, backend), streamer_(streamer) {
    auto propServer = owner->getPropertyServer();

    auto accelSampleRateList     = propServer->getStructureDataT<AccelSampleRateList>(OB_STRUCT_GET_ACCEL_PRESETS_ODR_LIST);
    auto accelFullScaleRangeList = propServer->getStructureDataT<AccelFullScaleRangeList>(OB_STRUCT_GET_ACCEL_PRESETS_FULL_SCALE_LIST);

    auto lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_ACCEL);
    for(uint32_t i = 0; i < accelSampleRateList.num; i++) {
        for(uint32_t j = 0; j < accelFullScaleRangeList.num; j++) {
            auto accel_odr = accelSampleRateList.odr[i];
            auto accel_fs  = accelFullScaleRangeList.fs[j];
            if(accel_odr == 0 || accel_fs == 0) {
                continue;
            }

            auto profile = StreamProfileFactory::createAccelStreamProfile(lazySensor, accel_fs, accel_odr);
            streamProfileList_.emplace_back(profile);
        }
    }
    LOG_DEBUG("AccelSensor is created!");
}

AccelSensor::~AccelSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("AccelSensor is destroyed");
}

void AccelSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    auto accelSp = sp->as<AccelStreamProfile>();
    propServer->setPropertyValueT(OB_PROP_ACCEL_ODR_INT, static_cast<int>(accelSp->getSampleRate()));
    propServer->setPropertyValueT(OB_PROP_ACCEL_FULL_SCALE_INT, static_cast<int>(accelSp->getFullScaleRange()));
    propServer->setPropertyValueT(OB_PROP_ACCEL_SWITCH_BOOL, true);

    BEGIN_TRY_EXECUTE({
        streamer_->start(sp, [this](std::shared_ptr<Frame> frame) {
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

void AccelSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stop(activatedStreamProfile_);
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    propServer->setPropertyValueT(OB_PROP_ACCEL_SWITCH_BOOL, false);
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor
