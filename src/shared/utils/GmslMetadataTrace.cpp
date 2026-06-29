// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GmslMetadataTrace.hpp"

#include "logger/Logger.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

namespace libobsensor {
namespace utils {
namespace GmslMetadataTrace {

namespace {

constexpr size_t G330_UVC_HEADER_BYTES = 12;

constexpr size_t G330_FRAME_COUNTER_OFFSET = 0;
constexpr size_t G330_SOF_SEC_OFFSET       = 12;
constexpr size_t G330_SOF_NSEC_OFFSET      = 16;
constexpr size_t G330_EXPOSURE_OFFSET      = 20;
constexpr size_t G330_BITMAP_OFFSET        = 52;
constexpr size_t G330_OFFSET_USEC_OFFSET   = 56;

struct MetadataFields {
    bool valid = false;

    uint32_t frameCounter = 0;
    uint32_t sofSec       = 0;
    uint32_t sofNsec      = 0;
    uint32_t exposure     = 0;
    uint32_t bitmap       = 0;
    uint32_t offsetUsec   = 0;
};

struct ModifierState {
    bool     hasLastFrameCounter = false;
    uint32_t lastFrameCounter    = 0;
    uint64_t lastSdkNumber       = 0;
    uint32_t sampleCount         = 0;
};

std::mutex                          modifierMutex;
std::map<OBFrameType, ModifierState> modifierStates;

uint32_t readLe32(const void *data, size_t size, size_t offset) {
    if(!data || offset + 4 > size) {
        return 0;
    }

    auto ptr = static_cast<const uint8_t *>(data) + offset;
    return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) | (static_cast<uint32_t>(ptr[2]) << 16)
           | (static_cast<uint32_t>(ptr[3]) << 24);
}

uint8_t readG330PrependedByte(const void *data, size_t size, OBStreamType streamType, size_t metadataOffset) {
    if(!data) {
        return 0;
    }

    auto ptr = static_cast<const uint8_t *>(data);
    if(streamType == OB_STREAM_COLOR || streamType == OB_STREAM_COLOR_LEFT || streamType == OB_STREAM_COLOR_RIGHT) {
        const size_t srcOffset = metadataOffset * 2 + 1;
        return srcOffset < size ? ptr[srcOffset] : 0;
    }
    if(streamType == OB_STREAM_DEPTH) {
        const size_t srcOffset = metadataOffset * 2;
        return srcOffset < size ? ptr[srcOffset] : 0;
    }
    return metadataOffset < size ? ptr[metadataOffset] : 0;
}

uint32_t readG330PrependedLe32(const void *data, size_t size, OBStreamType streamType, size_t metadataOffset) {
    return static_cast<uint32_t>(readG330PrependedByte(data, size, streamType, metadataOffset))
           | (static_cast<uint32_t>(readG330PrependedByte(data, size, streamType, metadataOffset + 1)) << 8)
           | (static_cast<uint32_t>(readG330PrependedByte(data, size, streamType, metadataOffset + 2)) << 16)
           | (static_cast<uint32_t>(readG330PrependedByte(data, size, streamType, metadataOffset + 3)) << 24);
}

MetadataFields readG330PrependedMetadata(const void *data, size_t size, OBStreamType streamType) {
    MetadataFields fields;
    if(!data || size == 0) {
        return fields;
    }

    fields.valid        = true;
    fields.frameCounter = readG330PrependedLe32(data, size, streamType, G330_FRAME_COUNTER_OFFSET);
    fields.sofSec       = readG330PrependedLe32(data, size, streamType, G330_SOF_SEC_OFFSET);
    fields.sofNsec      = readG330PrependedLe32(data, size, streamType, G330_SOF_NSEC_OFFSET);
    fields.exposure     = readG330PrependedLe32(data, size, streamType, G330_EXPOSURE_OFFSET);
    fields.bitmap       = readG330PrependedLe32(data, size, streamType, G330_BITMAP_OFFSET);
    fields.offsetUsec   = readG330PrependedLe32(data, size, streamType, G330_OFFSET_USEC_OFFSET);
    return fields;
}

MetadataFields readG330FrameMetadata(const uint8_t *metadata, size_t size) {
    MetadataFields fields;
    if(!metadata || size < G330_UVC_HEADER_BYTES + G330_OFFSET_USEC_OFFSET + sizeof(uint32_t)) {
        return fields;
    }

    fields.valid        = true;
    fields.frameCounter = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_FRAME_COUNTER_OFFSET);
    fields.sofSec       = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_SOF_SEC_OFFSET);
    fields.sofNsec      = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_SOF_NSEC_OFFSET);
    fields.exposure     = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_EXPOSURE_OFFSET);
    fields.bitmap       = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_BITMAP_OFFSET);
    fields.offsetUsec   = readLe32(metadata, size, G330_UVC_HEADER_BYTES + G330_OFFSET_USEC_OFFSET);
    return fields;
}

const char *metadataFormatName(MetadataFormat format) {
    switch(format) {
    case MetadataFormat::G330_96B:
        return "g330_96b";
    default:
        return "unknown";
    }
}

MetadataFields readPrependedMetadata(MetadataFormat format, const void *data, size_t size, OBStreamType streamType) {
    switch(format) {
    case MetadataFormat::G330_96B:
        return readG330PrependedMetadata(data, size, streamType);
    default:
        return {};
    }
}

void copySnapshot(MetadataSnapshot &snapshot, uint32_t index, uint32_t sequence, uint32_t bytesUsed, const void *data, size_t size) {
    snapshot.valid     = true;
    snapshot.index     = index;
    snapshot.sequence  = sequence;
    snapshot.bytesUsed = bytesUsed;
    snapshot.copied    = 0;

    if(!data) {
        return;
    }

    snapshot.copied = static_cast<uint32_t>(std::min(std::min(static_cast<size_t>(bytesUsed), size), METADATA_SNAPSHOT_BYTES));
    if(snapshot.copied > 0) {
        std::memcpy(snapshot.bytes, data, snapshot.copied);
    }
}

bool snapshotEquals(const MetadataSnapshot &snapshot, const void *data, size_t size) {
    if(!snapshot.valid || !data || snapshot.copied == 0 || size < snapshot.copied) {
        return false;
    }
    return std::memcmp(snapshot.bytes, data, snapshot.copied) == 0;
}

std::string hexPrefix(const void *data, size_t size, size_t maxBytes) {
    if(!data || size == 0 || maxBytes == 0) {
        return "";
    }

    static constexpr char HEX[] = "0123456789abcdef";
    auto                  ptr   = static_cast<const uint8_t *>(data);
    const size_t          count = std::min(size, maxBytes);

    std::string out;
    out.reserve(count * 3);
    for(size_t i = 0; i < count; ++i) {
        if(i > 0) {
            out.push_back(':');
        }
        out.push_back(HEX[(ptr[i] >> 4) & 0x0f]);
        out.push_back(HEX[ptr[i] & 0x0f]);
    }
    return out;
}

void appendFlag(std::string &flags, const char *flag) {
    if(!flags.empty()) {
        flags.push_back('|');
    }
    flags += flag;
}

int64_t sofUsec(const MetadataFields &fields) {
    return static_cast<int64_t>(fields.sofSec) * 1000000 + static_cast<int64_t>(fields.sofNsec) / 1000 - static_cast<int64_t>(fields.offsetUsec);
}

}  // namespace

bool isEnabled() {
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

V4lTrace::V4lTrace(std::string devName, OBStreamType streamType, size_t metadataBufferCount, MetadataFormat metadataFormat)
    : devName_(std::move(devName)), streamType_(streamType), metadataFormat_(metadataFormat), enabled_(isEnabled()), snapshots_(metadataBufferCount) {}

bool V4lTrace::enabled() const {
    return enabled_;
}

void V4lTrace::recordMetadataDequeued(uint32_t index, uint32_t sequence, uint32_t bytesUsed, const void *data, size_t size) {
    if(!enabled_ || index >= snapshots_.size()) {
        return;
    }
    copySnapshot(snapshots_[index], index, sequence, bytesUsed, data, size);
}

void V4lTrace::logFrame(uint64_t loopIndex, uint32_t videoSeq, uint32_t videoBytes, uint32_t videoIndex, int metadataIndex, uint32_t metadataSeq,
                        uint32_t metadataBytes, uint32_t payloadDwPresentationTime, const void *metadataData, size_t metadataSize, const void *imageData,
                        size_t imageSize) {
    if(!enabled_) {
        return;
    }

    const auto embeddedFields = readPrependedMetadata(metadataFormat_, imageData, imageSize, streamType_);

    const bool videoSeqJump = hasLastVideoSeq_ && videoSeq != lastVideoSeq_ + 1;
    const bool metaSeqJump  = metadataIndex >= 0 && hasLastMetadataSeq_ && metadataSeq != lastMetadataSeq_ + 1;
    const bool loopJump     = sampleCount_ > 0 && loopIndex != lastLoopIndex_ + 1;
    const bool embeddedCounterRepeat = embeddedFields.valid && hasLastEmbeddedCounter_ && embeddedFields.frameCounter == lastEmbeddedCounter_;
    const bool embeddedCounterJump = embeddedFields.valid && hasLastEmbeddedCounter_ && embeddedFields.frameCounter > lastEmbeddedCounter_ + 1;

    const MetadataSnapshot *snapshot = nullptr;
    if(metadataIndex >= 0 && static_cast<size_t>(metadataIndex) < snapshots_.size() && snapshots_[metadataIndex].valid) {
        snapshot = &snapshots_[metadataIndex];
    }
    const bool snapshotLaterEqual = snapshot && snapshotEquals(*snapshot, metadataData, metadataSize);
    const bool snapshotLaterDiff  = snapshot && metadataData && !snapshotLaterEqual;

    const bool shouldLog = sampleCount_ < 12 || videoSeqJump || metaSeqJump || loopJump || embeddedCounterRepeat || embeddedCounterJump || snapshotLaterDiff;
    if(shouldLog) {
        std::string flags;
        if(videoSeqJump) {
            appendFlag(flags, "video_seq_jump");
        }
        if(metaSeqJump) {
            appendFlag(flags, "meta_seq_jump");
        }
        if(loopJump) {
            appendFlag(flags, "loop_jump");
        }
        if(embeddedCounterRepeat) {
            appendFlag(flags, "embedded_counter_repeat");
        }
        if(embeddedCounterJump) {
            appendFlag(flags, "embedded_counter_jump");
        }
        if(snapshotLaterDiff) {
            appendFlag(flags, "dqbuf_later_hex_diff");
        }

        const auto dVideoSeq       = hasLastVideoSeq_ ? static_cast<int64_t>(videoSeq) - static_cast<int64_t>(lastVideoSeq_) : 0;
        const auto dMetadataSeq    = hasLastMetadataSeq_ ? static_cast<int64_t>(metadataSeq) - static_cast<int64_t>(lastMetadataSeq_) : 0;
        const auto dEmbeddedCount  = hasLastEmbeddedCounter_ ? static_cast<int64_t>(embeddedFields.frameCounter) - static_cast<int64_t>(lastEmbeddedCounter_) : 0;
        const auto embeddedSofTime = embeddedFields.valid ? sofUsec(embeddedFields) : 0;
        const auto dEmbeddedSofMs  = hasLastEmbeddedCounter_ ? (embeddedSofTime - lastEmbeddedSofUsec_) / 1000.0 : 0.0;

        LOG_INFO("GMSL metadata trace source=v4l dev={} streamType={} loopIndex={} videoSeq={} dVideoSeq={} videoBufIndex={} videoBytes={} "
                  "metadataIndex={} metaSeq={} dMetaSeq={} metaLen={} metaFirstU32={} payloadDwPresentationTime={} dqbufMetaSeq={} dqbufMetaLen={} "
                  "dqbufMetaCopied={} dqbufMetaHex={} laterMetaHex={} dqbufLaterHexEqual={} embeddedFormat={} embeddedFrameCounter={} "
                  "dEmbeddedFrameCounter={} embeddedSofSec={} embeddedSofNsec={} embeddedExposure={} embeddedBitmap=0x{:x} embeddedOffsetUsec={} "
                  "embeddedSofUsec={} dEmbeddedSofMs={} flags={}",
                  devName_, static_cast<int>(streamType_), loopIndex, videoSeq, dVideoSeq, videoIndex, videoBytes, metadataIndex, metadataSeq, dMetadataSeq,
                  metadataBytes, readLe32(metadataData, metadataSize, 0), payloadDwPresentationTime, snapshot ? snapshot->sequence : 0,
                  snapshot ? snapshot->bytesUsed : 0, snapshot ? snapshot->copied : 0,
                  snapshot ? hexPrefix(snapshot->bytes, snapshot->copied, METADATA_SNAPSHOT_BYTES) : "",
                  hexPrefix(metadataData, metadataSize, METADATA_SNAPSHOT_BYTES), snapshot ? (snapshotLaterEqual ? 1 : 0) : -1, metadataFormatName(metadataFormat_),
                  embeddedFields.frameCounter, dEmbeddedCount, embeddedFields.sofSec, embeddedFields.sofNsec, embeddedFields.exposure,
                  embeddedFields.bitmap, embeddedFields.offsetUsec, embeddedSofTime, dEmbeddedSofMs, flags.empty() ? "none" : flags.c_str());
    }

    hasLastVideoSeq_ = true;
    lastVideoSeq_    = videoSeq;
    if(metadataIndex >= 0) {
        hasLastMetadataSeq_ = true;
        lastMetadataSeq_    = metadataSeq;
    }
    if(embeddedFields.valid) {
        hasLastEmbeddedCounter_ = true;
        lastEmbeddedCounter_    = embeddedFields.frameCounter;
        lastEmbeddedSofUsec_    = sofUsec(embeddedFields);
    }
    lastLoopIndex_ = loopIndex;
    sampleCount_++;
}

void logG330ModifierFrame(OBFrameType frameType, uint64_t sdkNumber, const uint8_t *metadata, size_t metadataSize) {
    if(!isEnabled()) {
        return;
    }

    const auto fields = readG330FrameMetadata(metadata, metadataSize);

    std::lock_guard<std::mutex> lock(modifierMutex);
    auto                       &state = modifierStates[frameType];
    const bool                  repeated = fields.valid && state.hasLastFrameCounter && fields.frameCounter == state.lastFrameCounter;
    const bool                  jumped   = fields.valid && state.hasLastFrameCounter && fields.frameCounter > state.lastFrameCounter + 1;
    const bool                  shouldLog = state.sampleCount < 12 || repeated || jumped;

    if(shouldLog) {
        std::string flags;
        if(repeated) {
            appendFlag(flags, "frame_counter_repeat");
        }
        if(jumped) {
            appendFlag(flags, "frame_counter_jump");
        }

        LOG_INFO("GMSL metadata trace source=modifier metadataFormat=g330 frameType={} sdkNumber={} dSdkNumber={} mdFrameCounter={} dMdFrameCounter={} "
                  "sofSec={} sofNsec={} exposure={} bitmap=0x{:x} offsetUsec={} flags={}",
                  static_cast<int>(frameType), sdkNumber,
                  state.sampleCount > 0 ? static_cast<int64_t>(sdkNumber) - static_cast<int64_t>(state.lastSdkNumber) : 0, fields.frameCounter,
                  state.hasLastFrameCounter ? static_cast<int64_t>(fields.frameCounter) - static_cast<int64_t>(state.lastFrameCounter) : 0, fields.sofSec,
                  fields.sofNsec, fields.exposure, fields.bitmap, fields.offsetUsec, flags.empty() ? "none" : flags.c_str());
    }

    if(fields.valid) {
        state.hasLastFrameCounter = true;
        state.lastFrameCounter    = fields.frameCounter;
    }
    state.lastSdkNumber = sdkNumber;
    state.sampleCount++;
}

}  // namespace GmslMetadataTrace
}  // namespace utils
}  // namespace libobsensor
