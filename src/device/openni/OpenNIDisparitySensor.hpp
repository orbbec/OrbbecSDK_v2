// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/video/VideoSensor.hpp"

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

protected:
    void outputFrame(std::shared_ptr<Frame> frame) override;

    void convertProfileAsDisparityBasedProfile();

protected:
    bool outputDisparityFrame_ = false;

    float depthUnit_ = 1.0f;
};
}  // namespace libobsensor
