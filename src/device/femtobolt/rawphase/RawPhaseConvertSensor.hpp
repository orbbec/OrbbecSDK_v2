#pragma once

#include "sensor/SensorBase.hpp"
#include "sensor/video/VideoSensor.hpp"
#include "RawPhaseStreamer.hpp"

namespace libobsensor {
class RawPhaseConvertSensor : public VideoSensor {
public:
    RawPhaseConvertSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, OBSensorType sensorType,
                          const std::shared_ptr<RawPhaseStreamer> &streamer);

    ~RawPhaseConvertSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;
    void      updateStreamProfileList(const StreamProfileList &profileList) override;


private:
    void updateStreamProfileList();

private:
    std::shared_ptr<RawPhaseStreamer>                                                    streamer_;
    OBSensorType                                                                         sensorType_;
    std::vector<std::pair<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>>> profileVector_;
    bool                                                                                 isPassiveIR_ = false;
};
}  // namespace libobsensor
