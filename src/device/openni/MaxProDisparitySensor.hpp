
#pragma once

#include "sensor/video/VideoSensor.hpp"

namespace libobsensor {
class MaxProDisparitySensor : public VideoSensor {
public:
    MaxProDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~MaxProDisparitySensor() override = default;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;

    void updateFormatFilterConfig(const std::vector<FormatFilterConfig> &configs) override;

    void setDepthUnit(float unit);

    void initProfileVirtualRealMap();

    void setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor);

private:
    void outputFrame(std::shared_ptr<Frame> frame) override;

    void convertProfileAsDisparityBasedProfile();

    void syncDisparityToDepthModeStatus();

private:
    bool  isCropStreamProfile_ = false;
    float depthUnit_           = 1.0f;

    std::map<std::shared_ptr<const StreamProfile>, std::shared_ptr<const StreamProfile>> profileVirtualRealMap_;

    std::shared_ptr<const StreamProfile> realActivatedStreamProfile_;
};
}  // namespace libobsensor