// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330MetadataModifier.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {

G330GMSLMetadataModifier::G330GMSLMetadataModifier(IDevice *owner) : DeviceComponentBase(owner) {}
G330GMSLMetadataModifier::~G330GMSLMetadataModifier() {}

void G330GMSLMetadataModifier::modify(std::shared_ptr<Frame> frame) {
    constexpr const int metadataSize = 96;

    frame->setMetadataSize(metadataSize + 12);  // 12 bytes for front padding, useless data
    uint8_t *metadataBuffer = frame->getMetadataMutable();

    switch(frame->getType()) {
    case OB_FRAME_COLOR: {
        // Copy metadata from the frame data without clearing the metadata area
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frame->getData());
        for(int i = 0; i < metadataSize; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] >> 8);
        }
    } break;
    case OB_FRAME_DEPTH: {
        // Copy metadata from the frame data and clear the metadata area
        auto            frameData       = frame->getDataMutable();
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frameData);
        for(int i = 0; i < metadataSize; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] & 0x00ff);
        }
        // Zeroing the metadata region prevents horizontal stripes in display
        // For depth images, zeroed pixels are interpreted as depth=0 and do not affect usage
        memset(frameData, 0, metadataSize * sizeof(uint16_t));
    } break;
    case OB_FRAME_IR:
    case OB_FRAME_IR_LEFT:
    case OB_FRAME_IR_RIGHT: {
        // Copy metadata from the frame data and clear the metadata area
        auto frameData = frame->getDataMutable();
        memcpy(metadataBuffer + 12, frameData, metadataSize);
        // Zeroing the metadata region prevents horizontal stripes in display
        memset(frameData, 0, metadataSize);
    } break;
    default:
        // do nothing
        break;
    }
}

}  // namespace libobsensor