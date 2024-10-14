// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RawPhaseBasedSensor.hpp"
#include "RawPhaseStreamer.hpp"
#include "property/InternalProperty.hpp"
namespace libobsensor {
RawPhaseBasedSensor::RawPhaseBasedSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {}

RawPhaseBasedSensor::~RawPhaseBasedSensor() {}

void RawPhaseBasedSensor::trySendStopStreamVendorCmd() {
    auto rawPhaseStreamer = std::dynamic_pointer_cast<RawPhaseStreamer>(backend_);
    if(rawPhaseStreamer->isRunning()) {
        return;
    }
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    if(propServer->isPropertySupported(OB_PROP_STOP_DEPTH_STREAM_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->setPropertyValueT<bool>(OB_PROP_STOP_DEPTH_STREAM_BOOL, true);
    }
}

void RawPhaseBasedSensor::refreshStreamProfiles() {
    auto streamProfileList = getStreamProfileList();
    auto defaultSp         = streamProfileList.front();

    auto owner         = getOwner();
    auto vsPort        = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    auto lazySelf      = std::make_shared<LazySensor>(owner, sensorType_);
    auto streamType    = utils::mapSensorTypeToStreamType(sensorType_);
    auto backendSpList = vsPort->getStreamProfileList();
    for(auto &backendSp: backendSpList) {
        auto sp = backendSp->clone();
        sp->bindOwner(lazySelf);
        sp->setType(streamType);
        backendStreamProfileList_.push_back(sp);
        LOG_DEBUG("Backend stream profile {}", backendSp);
    }

    std::sort(backendStreamProfileList_.begin(), backendStreamProfileList_.end(),
              [](const std::shared_ptr<const StreamProfile> &a, const std::shared_ptr<const StreamProfile> &b) {
                  auto aVsp = a->as<VideoStreamProfile>();
                  auto bVsp = b->as<VideoStreamProfile>();
                  auto aRes = aVsp->getWidth() * aVsp->getHeight();
                  auto bRes = bVsp->getWidth() * bVsp->getHeight();
                  if(aRes != bRes) {
                      return aRes > bRes;
                  }
                  else if(aVsp->getHeight() != bVsp->getHeight()) {
                      return aVsp->getHeight() > bVsp->getHeight();
                  }
                  else if(aVsp->getFps() != bVsp->getFps()) {
                      return aVsp->getFps() > bVsp->getFps();
                  }
                  return aVsp->getFormat() > bVsp->getFormat();
              });

    // The stream profile list is same as the backend stream profile list at default.
    for(auto &backendSp: backendStreamProfileList_) {
        auto sp = backendSp->clone();
        sp->bindOwner(lazySelf);
        sp->setType(streamType);
        streamProfileList_.push_back(sp);
        streamProfileBackendMap_[sp] = { backendSp, nullptr };
    }

    updateFormatFilterConfig(formatFilterConfigs_);
    updateDefaultStreamProfile(defaultSp);
}

}  // namespace libobsensor
