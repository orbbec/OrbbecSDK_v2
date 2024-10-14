// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "stream/StreamProfile.hpp"
#include <libobsensor/h/ObTypes.h>
#include <vector>
#include <memory>

namespace libobsensor {

class Config {
public:
    Config()           = default;
    ~Config() noexcept = default;

    void enableStream(std::shared_ptr<const StreamProfile> prf);
    void enableStream(OBStreamType type);
    void enableVideoStream(OBStreamType type, uint32_t width, uint32_t height, uint32_t fps, OBFormat format);
    void enableAccelStream(OBAccelFullScaleRange fullScaleRange, OBAccelSampleRate sampleRate);
    void enableGyroStream(OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate);

    void disableStream(OBStreamType type);
    void disableAllStream();

    bool                                 isStreamEnabled(OBStreamType type) const;
    StreamProfileList                    getEnabledStreamProfileList() const;
    std::shared_ptr<const StreamProfile> getEnabledStreamProfile(OBStreamType type) const;

    void        setAlignMode(OBAlignMode mode);
    OBAlignMode getAlignMode() const;
    void        setDepthScaleAfterAlignRequire(bool enable);
    bool        getDepthScaleAfterAlignRequire() const;

    void                       setFrameAggregateOutputMode(OBFrameAggregateOutputMode mode);
    OBFrameAggregateOutputMode getFrameAggregateOutputMode() const;

    bool operator==(const Config &cmp) const;
    bool operator!=(const Config &cmp) const;

    std::shared_ptr<Config> clone() const;

private:
    StreamProfileList          enabledStreamProfileList_;
    OBAlignMode                alignMode_{ ALIGN_DISABLE };
    bool                       depthScaleRequire_        = true;
    OBFrameAggregateOutputMode frameAggregateOutputMode_ = OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION;
};
}  // namespace libobsensor
