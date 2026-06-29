// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330MetadataModifier.hpp"
#include "frame/Frame.hpp"
#include "logger/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace libobsensor {

namespace {
constexpr size_t kGmslMetadataTraceFrameCounterOffset = 12;
constexpr size_t kGmslMetadataTraceSofSecOffset       = 24;
constexpr size_t kGmslMetadataTraceSofNsecOffset      = 28;
constexpr size_t kGmslMetadataTraceExposureOffset     = 32;
constexpr size_t kGmslMetadataTraceBitmapOffset       = 64;
constexpr size_t kGmslMetadataTraceOffsetUsecOffset   = 68;

uint32_t readLe32GmslMetadataTrace(const uint8_t *data, size_t size, size_t offset) {
    if(!data || offset + 4 > size) {
        return 0;
    }
    auto ptr = data + offset;
    return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) | (static_cast<uint32_t>(ptr[2]) << 16)
           | (static_cast<uint32_t>(ptr[3]) << 24);
}

struct GmslMetadataTraceModifierState {
    bool     hasLastFrameCounter = false;
    uint32_t lastFrameCounter    = 0;
    uint64_t lastSdkNumber       = 0;
    uint32_t sampleCount         = 0;
};

std::mutex                                      gmslMetadataTraceModifierMutex;
std::map<OBFrameType, GmslMetadataTraceModifierState> gmslMetadataTraceModifierStates;

bool isGmslMetadataTraceEnabled() {
    static const bool enabled = [] {
        const char *value = std::getenv("OB_GMSL_METADATA_TRACE");
        if(!value) {
            return false;
        }
        return std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0 || std::strcmp(value, "TRUE") == 0 || std::strcmp(value, "on") == 0
               || std::strcmp(value, "ON") == 0;
    }();
    return enabled;
}

void logGmslMetadataTraceModifier(const std::shared_ptr<Frame> &frame, const uint8_t *metadataBuffer, size_t metadataSize) {
    if(!isGmslMetadataTraceEnabled()) {
        return;
    }

    const uint32_t frameCounter = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceFrameCounterOffset);
    const uint32_t sofSec       = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceSofSecOffset);
    const uint32_t sofNsec      = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceSofNsecOffset);
    const uint32_t exposure     = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceExposureOffset);
    const uint32_t bitmap       = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceBitmapOffset);
    const uint32_t offsetUsec   = readLe32GmslMetadataTrace(metadataBuffer, metadataSize, kGmslMetadataTraceOffsetUsecOffset);

    std::lock_guard<std::mutex> lock(gmslMetadataTraceModifierMutex);
    auto                       &state = gmslMetadataTraceModifierStates[frame->getType()];
    const bool repeated = state.hasLastFrameCounter && frameCounter == state.lastFrameCounter;
    const bool jumped   = state.hasLastFrameCounter && frameCounter > state.lastFrameCounter + 1;
    const bool shouldLog = state.sampleCount < 12 || repeated || jumped;

    if(shouldLog) {
        std::string flags;
        if(repeated) {
            flags += " frame_counter_repeat";
        }
        if(jumped) {
            flags += " frame_counter_jump";
        }
        LOG_INFO("GMSL metadata trace source=modifier frameType={} sdkNumber={} dSdkNumber={} mdFrameCounter={} dMdFrameCounter={} sofSec={} sofNsec={} "
                 "exposure={} bitmap=0x{:x} offsetUsec={} flags={}",
                 static_cast<int>(frame->getType()), frame->getNumber(),
                 state.sampleCount > 0 ? static_cast<int64_t>(frame->getNumber()) - static_cast<int64_t>(state.lastSdkNumber) : 0, frameCounter,
                 state.hasLastFrameCounter ? static_cast<int64_t>(frameCounter) - static_cast<int64_t>(state.lastFrameCounter) : 0, sofSec, sofNsec,
                 exposure, bitmap, offsetUsec, flags.empty() ? "none" : flags.c_str());
    }

    state.hasLastFrameCounter = true;
    state.lastFrameCounter    = frameCounter;
    state.lastSdkNumber       = frame->getNumber();
    state.sampleCount++;
}
}  // namespace

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
        logGmslMetadataTraceModifier(frame, metadataBuffer, metadataSize + 12);
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
        logGmslMetadataTraceModifier(frame, metadataBuffer, metadataSize + 12);
    } break;
    case OB_FRAME_IR:
    case OB_FRAME_IR_LEFT:
    case OB_FRAME_IR_RIGHT: {
        // Copy metadata from the frame data and clear the metadata area
        auto frameData = frame->getDataMutable();
        memcpy(metadataBuffer + 12, frameData, metadataSize);
        // Zeroing the metadata region prevents horizontal stripes in display
        memset(frameData, 0, metadataSize);
        logGmslMetadataTraceModifier(frame, metadataBuffer, metadataSize + 12);
    } break;
    default:
        // do nothing
        break;
    }
}

}  // namespace libobsensor
