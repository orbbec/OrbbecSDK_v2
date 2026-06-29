// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace libobsensor {
namespace utils {
namespace GmslMetadataTrace {

constexpr size_t METADATA_SNAPSHOT_BYTES = 96;

bool isEnabled();

enum class MetadataFormat {
    G330_96B,
};

struct MetadataSnapshot {
    bool valid = false;

    uint32_t index     = 0;
    uint32_t sequence  = 0;
    uint32_t bytesUsed = 0;
    uint32_t copied    = 0;

    uint8_t bytes[METADATA_SNAPSHOT_BYTES] = {};
};

class V4lTrace {
public:
    V4lTrace(std::string devName, OBStreamType streamType, size_t metadataBufferCount, MetadataFormat metadataFormat = MetadataFormat::G330_96B);

    bool enabled() const;

    void recordMetadataDequeued(uint32_t index, uint32_t sequence, uint32_t bytesUsed, const void *data, size_t size);

    void logFrame(uint64_t loopIndex, uint32_t videoSeq, uint32_t videoBytes, uint32_t videoIndex, int metadataIndex, uint32_t metadataSeq,
                  uint32_t metadataBytes, uint32_t payloadDwPresentationTime, const void *metadataData, size_t metadataSize, const void *imageData,
                  size_t imageSize);

private:
    std::string                   devName_;
    OBStreamType                  streamType_;
    MetadataFormat                metadataFormat_;
    bool                          enabled_ = false;
    std::vector<MetadataSnapshot> snapshots_;

    bool     hasLastVideoSeq_        = false;
    bool     hasLastMetadataSeq_     = false;
    bool     hasLastEmbeddedCounter_ = false;
    uint32_t lastVideoSeq_           = 0;
    uint32_t lastMetadataSeq_        = 0;
    uint32_t lastEmbeddedCounter_    = 0;
    int64_t  lastEmbeddedSofUsec_    = 0;
    uint64_t lastLoopIndex_          = 0;
    uint32_t sampleCount_            = 0;
};

void logG330ModifierFrame(OBFrameType frameType, uint64_t sdkNumber, const uint8_t *metadata, size_t metadataSize);

}  // namespace GmslMetadataTrace
}  // namespace utils
}  // namespace libobsensor
