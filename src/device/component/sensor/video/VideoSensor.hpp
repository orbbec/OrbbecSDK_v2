// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "sensor/SensorBase.hpp"
#include "IFrame.hpp"
#include "IFilter.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>

namespace libobsensor {

enum class FormatFilterPolicy {
    REMOVE,   // Directly remove the stream profile if the format matches the src format
    REPLACE,  // Replace the stream profile with a new format stream profile. If there is a frame format converter available, use it to convert the frame to the
              // new format
    ADD,      // Use the frame format converter to convert the frame to the new format and add a new format stream profile to the list
};

struct FormatFilterConfig {
    FormatFilterPolicy       policy;
    OBFormat                 srcFormat;
    OBFormat                 dstFormat;
    std::shared_ptr<IFilter> converter;
};

class VideoSensor : public SensorBase {
public:
    VideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);

    virtual ~VideoSensor() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

    virtual void updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs);
    void         setStreamProfileList(const StreamProfileList &profileList) override;
    void         setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor);

protected:
    virtual void trySendStopStreamVendorCmd();
    void         onBackendFrameCallback(std::shared_ptr<Frame> frame);
    void         outputFrame(std::shared_ptr<Frame> frame) override;

protected:
    typedef std::pair<std::shared_ptr<const StreamProfile>, const FormatFilterConfig *> StreamProfileBackendMapValue;
    std::map<std::shared_ptr<const StreamProfile>, StreamProfileBackendMapValue>        streamProfileBackendMap_;

    std::vector<FormatFilterConfig>      formatFilterConfigs_;
    const FormatFilterConfig            *currentFormatFilterConfig_;
    std::shared_ptr<const StreamProfile> currentBackendStreamProfile_;
    StreamProfileList                    backendStreamProfileList_;

    std::shared_ptr<FrameProcessor> frameProcessor_;
};

}  // namespace libobsensor

