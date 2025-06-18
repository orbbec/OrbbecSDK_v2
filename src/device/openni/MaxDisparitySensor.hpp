#pragma once

#include "OpenNIDisparitySensor.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

class MaxDisparitySensor : public OpenNIDisparitySensor {
public:
    MaxDisparitySensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~MaxDisparitySensor() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;

    void initProfileVirtualRealMap();

private:
    std::map<std::shared_ptr<const StreamProfile>, std::shared_ptr<const StreamProfile>> profileVirtualRealMap_;
    std::map<std::shared_ptr<const StreamProfile>, OpenNIFrameProcessParam>              profileProcessParamMap_;

};
}  // namespace libobs

