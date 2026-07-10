// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G305MetadataModifier.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {

G305GMSLMetadataModifier::G305GMSLMetadataModifier(IDevice *owner) : DeviceComponentBase(owner) {}
G305GMSLMetadataModifier::~G305GMSLMetadataModifier() {}

void G305GMSLMetadataModifier::modify(std::shared_ptr<Frame> frame) {
    constexpr const int metadataSize = 96;

    frame->setMetadataSize(metadataSize + 12);  // 12 bytes for front padding, useless data
    uint8_t *metadataBuffer = frame->getMetadataMutable();

    switch(frame->getType()) {
    case OB_FRAME_COLOR:
    case OB_FRAME_COLOR_LEFT: {
        // Copy metadata from the frame data
        auto            frameData       = frame->getDataMutable();
        const uint16_t *frameDataBuffer = reinterpret_cast<const uint16_t *>(frameData);
        for(int i = 0; i < metadataSize; i++) {
            metadataBuffer[i + 12] = static_cast<uint8_t>(frameDataBuffer[i] >> 8);
        }
        // The metadata bytes overwrite pixel data in the first row and cannot be restored;
        // conceal them by copying the same-position pixels from the second row.
        // Color raw is 2 bytes per pixel (e.g. YUYV), so one row spans width * 2 bytes.
        const uint32_t rowStride = frame->as<VideoFrame>()->getWidth() * 2;
        if(rowStride >= metadataSize * sizeof(uint16_t)) {
            memcpy(frameData, frameData + rowStride, metadataSize * sizeof(uint16_t));
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
    case OB_FRAME_COLOR_RIGHT: {
        // Right color is raw8 with 2x width, metadata is packed as raw8 bytes with swapped byte pairs
        auto frameData = frame->getDataMutable();
        memcpy(metadataBuffer + 12, frameData, metadataSize);
        // Swap high and low bytes within each uint16 to match metadata structure byte order
        auto frameDataBuffer = reinterpret_cast<uint16_t *>(metadataBuffer + 12);
        for(int i = 0; i < metadataSize / 2; i++) {
            frameDataBuffer[i] = static_cast<uint16_t>((frameDataBuffer[i] << 8) | (frameDataBuffer[i] >> 8));
        }
        // The metadata bytes overwrite pixel data in the first row and cannot be restored;
        // conceal them by copying the same-position pixels from the second row.
        // Color raw is 2 bytes per pixel (e.g. YUYV), so one row spans width * 2 bytes.
        const uint32_t rowStride = frame->as<VideoFrame>()->getWidth() * 2;
        if(rowStride >= metadataSize) {
            memcpy(frameData, frameData + rowStride, metadataSize);
        }
    } break;
    default:
        // do nothing
        break;
    }
}

}  // namespace libobsensor