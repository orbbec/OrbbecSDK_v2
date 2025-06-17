// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/video/VideoSensor.hpp"
#include <vector>

namespace libobsensor {
class OpenNIDisparitySensor : public VideoSensor {
public:
    OpenNIDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~OpenNIDisparitySensor() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;

    void updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) override;

    void markOutputDisparityFrame(bool enable);

    void setDepthUnit(float unit);

    void setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor);

    OpenNIFrameProcessParam getCurrentFrameProcessParam() {
        return currentProcessParam_;
    }

    void setFrameProcessParam(OpenNIFrameProcessParam currentProcessParam) {
        playbackProcessParam_.clear();
        playbackProcessParam_.push_back(currentProcessParam);
    }

protected:
    void outputFrame(std::shared_ptr<Frame> frame) override;

    void convertProfileAsDisparityBasedProfile();

protected:
    bool outputDisparityFrame_ = false;

    float depthUnit_ = 1.0f;

    OpenNIFrameProcessParam              currentProcessParam_ = { 1, 0, 0, 0, 0, 0, 0 };
    std::vector<OpenNIFrameProcessParam> playbackProcessParam_;

};
}  // namespace libobsensor
