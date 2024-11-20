// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DabaiAMetadataModifier.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
#define DABAI_AL_GMSL_METADATA_SIZE 96

DabaiALGMSLMetadataModifier::DabaiALGMSLMetadataModifier(IDevice *owner) : DeviceComponentBase(owner) {}
DabaiALGMSLMetadataModifier::~DabaiALGMSLMetadataModifier() {}

void DabaiALGMSLMetadataModifier::modify(std::shared_ptr<Frame> frame) {
    frame->setMetadataSize(DABAI_AL_GMSL_METADATA_SIZE + 12);  // 12 bytes for front padding, useless data
    uint8_t *metadataBuffer = frame->getMetadataMutable();
    if(frame->getType() == OB_FRAME_COLOR) {
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frame->getData());
        for(int i = 0; i < DABAI_AL_GMSL_METADATA_SIZE; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] >> 8);
        }
    }
    else if(frame->getType() == OB_FRAME_DEPTH) {
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frame->getData());
        for(int i = 0; i < DABAI_AL_GMSL_METADATA_SIZE; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] & 0x00ff);
        }
    }
    else {
        const uint8_t *frameDataBuffer = frame->getData();
        memcpy(metadataBuffer + 12, frameDataBuffer, DABAI_AL_GMSL_METADATA_SIZE);
    }
}

}  // namespace libobsensor