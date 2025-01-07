// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <atomic>
#include <memory>
#include <functional>

#include "libobsensor/h/ObTypes.h"

namespace libobsensor {

class Frame;

typedef std::function<void(std::shared_ptr<const Frame>)> FrameCallback;
typedef std::function<void(std::shared_ptr<Frame>)>       MutableFrameCallback;

class IFrameMetadataParser {
public:
    virtual ~IFrameMetadataParser()                                       = default;
    virtual int64_t getValue(const uint8_t *metadata, size_t dataSize)    = 0;
    virtual bool    isSupported(const uint8_t *metadata, size_t dataSize) = 0;
};

typedef std::function<int64_t(const int64_t &param)> FrameMetadataModifier;

class IFrameMetadataParserContainer {
public:
    IFrameMetadataParserContainer() {}
    virtual ~IFrameMetadataParserContainer()                                                                                             = default;
    virtual void                                  registerParser(OBFrameMetadataType type, std::shared_ptr<IFrameMetadataParser> phaser) = 0;
    virtual bool                                  isContained(OBFrameMetadataType type)                                                  = 0;
    virtual std::shared_ptr<IFrameMetadataParser> get(OBFrameMetadataType type)                                                          = 0;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_frame_t {
    std::shared_ptr<libobsensor::Frame> frame;
    std::atomic<int>                    refCnt = { 1 };
};
#ifdef __cplusplus
}
#endif
