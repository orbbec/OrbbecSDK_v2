#pragma once

#include "OpenNIDisparitySensor.hpp"

namespace libobsensor {
typedef struct OpenNIFrameProcessParam {
    float scale;
    int   xCut;
    int   yCut;
    int   xSet;
    int   ySet;
    int   dstWidth;
    int   dstHeight;
} OpenNIFrameProcessParam;

class MaxDisparitySensor : public OpenNIDisparitySensor {
public:
    MaxDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~MaxDisparitySensor() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;

    void initProfileVirtualRealMap();

    void setFrameProcessor(std::shared_ptr<FrameProcessor> frameProcessor);

private:
    bool isCropStreamProfile_ = false;

    std::map<std::shared_ptr<const StreamProfile>, std::shared_ptr<const StreamProfile>> profileVirtualRealMap_;
    std::map<std::shared_ptr<const StreamProfile>, OpenNIFrameProcessParam>              profileProcessParamMap_;

    std::shared_ptr<const StreamProfile> realActivatedStreamProfile_;
};
}  // namespace libobs

