
#pragma once

#include "OpenNIDisparitySensor.hpp"

namespace libobsensor {
class MaxProDisparitySensor : public OpenNIDisparitySensor {
public:
    MaxProDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~MaxProDisparitySensor() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;

    void initProfileVirtualRealMap();

    void setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor);

protected:
    void outputFrame(std::shared_ptr<Frame> frame) override;

private:
    bool isCropStreamProfile_ = false;

    std::map<std::shared_ptr<const StreamProfile>, std::shared_ptr<const StreamProfile>> profileVirtualRealMap_;

    std::shared_ptr<const StreamProfile> realActivatedStreamProfile_;
};
}  // namespace libobsensor