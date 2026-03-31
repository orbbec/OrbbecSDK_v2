// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "StartOfExposureTimestampAdjuster.hpp"
#include "frame/Frame.hpp"
#include "IDevice.hpp"
#include "libobsensor/h/ObTypes.h"
namespace libobsensor {

StartOfExposureTimestampAdjuster::StartOfExposureTimestampAdjuster(IDevice *device) : DeviceComponentBase(device) {
    auto propServer = device->getPropertyServer();
    if(propServer->isPropertySupported(OB_PROP_INTRA_CAMERA_SYNC_REFERENCE_INT, PROP_OP_READ, PROP_ACCESS_USER)) {
        // Read once on initialization
        OBPropertyValue val = {};
        propServer->getPropertyValue(OB_PROP_INTRA_CAMERA_SYNC_REFERENCE_INT, &val, PROP_ACCESS_USER);
        syncReferenceMode_ = val.intValue;

        // Re-read from device whenever the user writes the property
        propServer->registerAccessCallback(OB_PROP_INTRA_CAMERA_SYNC_REFERENCE_INT,
                                           [this](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
                                               if(operationType == PROP_OP_WRITE) {
                                                   auto            propServer = getOwner()->getPropertyServer();
                                                   OBPropertyValue val        = {};
                                                   propServer->getPropertyValue(OB_PROP_INTRA_CAMERA_SYNC_REFERENCE_INT, &val, PROP_ACCESS_USER);
                                                   syncReferenceMode_ = val.intValue;
                                               }
                                           });
    }
}

void StartOfExposureTimestampAdjuster::calculate(std::shared_ptr<Frame> frame) {
    if(syncReferenceMode_.load() != OB_START_OF_EXPOSURE) {
        return;
    }
    if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_TIMESTAMP) && frame->hasMetadata(OB_FRAME_METADATA_TYPE_EXPOSURE)) {
        auto    ts  = frame->getMetadataValue(OB_FRAME_METADATA_TYPE_TIMESTAMP);
        auto    exp = frame->getMetadataValue(OB_FRAME_METADATA_TYPE_EXPOSURE);
        // Color exposure unit is 100us; depth/IR exposure unit is 1us
        auto    frameType = frame->getType();

        int64_t expUsec   = (isColorFrame(frameType)) ? exp * 100 : exp;
        if(ts > expUsec) {
            frame->setTimeStampUsec(static_cast<uint64_t>(ts - expUsec));
        }
    }
}

}  // namespace libobsensor
