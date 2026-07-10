// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DabaiAMetadataModifier.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
#define DABAI_AL_GMSL_METADATA_SIZE (96)

// Gemini345Lg embed metadata into the color pixel area (see DeviceInfoConfig.xml).
static const std::vector<std::pair<int32_t, int32_t>> gemini345LgVidPids = {
    { 0x2BC5, 0x0813 },  // Gemini345Lg
};

DabaiALGMSLMetadataModifier::DabaiALGMSLMetadataModifier(IDevice *owner) : DeviceComponentBase(owner) {
    auto info = owner->getInfo();
    for(const auto &vidPid: gemini345LgVidPids) {
        if(info->vid_ == vidPid.first && info->pid_ == vidPid.second) {
            needImageRepair_ = true;
            break;
        }
    }
}
DabaiALGMSLMetadataModifier::~DabaiALGMSLMetadataModifier() {}

void DabaiALGMSLMetadataModifier::modify(std::shared_ptr<Frame> frame) {
    frame->setMetadataSize(DABAI_AL_GMSL_METADATA_SIZE + 12);  // 12 bytes for front padding, useless data
    uint8_t *metadataBuffer = frame->getMetadataMutable();

    switch(frame->getType()) {
    case OB_FRAME_COLOR: {
        auto            frameData       = frame->getDataMutable();
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frameData);
        for(int i = 0; i < DABAI_AL_GMSL_METADATA_SIZE; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] >> 8);
        }
        // Conceal the metadata-overwritten first row by copying from the second row (2 bytes/pixel).
        if(needImageRepair_) {
            const uint32_t rowStride = frame->as<VideoFrame>()->getWidth() * 2;
            if(rowStride >= DABAI_AL_GMSL_METADATA_SIZE * sizeof(uint16_t)) {
                memcpy(frameData, frameData + rowStride, DABAI_AL_GMSL_METADATA_SIZE * sizeof(uint16_t));
            }
        }
    } break;
    case OB_FRAME_DEPTH: {
        auto            frameData       = frame->getDataMutable();
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frameData);
        for(int i = 0; i < DABAI_AL_GMSL_METADATA_SIZE; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] & 0x00ff);
        }
        if(needImageRepair_) {
            // Zeroing the metadata region prevents horizontal stripes in display
            // For depth images, zeroed pixels are interpreted as depth=0 and do not affect usage
            memset(frameData, 0, DABAI_AL_GMSL_METADATA_SIZE * sizeof(uint16_t));
        }
    } break;
    case OB_FRAME_IR_LEFT:
    case OB_FRAME_IR_RIGHT: {
        // Copy metadata from the frame data and clear the metadata area
        auto frameData = frame->getDataMutable();
        memcpy(metadataBuffer + 12, frameData, DABAI_AL_GMSL_METADATA_SIZE);

        if(needImageRepair_) {
            // Zeroing the metadata region prevents horizontal stripes in display
            memset(frameData, 0, DABAI_AL_GMSL_METADATA_SIZE);
        }
    }
    default:
        // do nothing
        break;
    }
}

}  // namespace libobsensor