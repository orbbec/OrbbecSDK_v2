// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISensorStreamStrategy.hpp"
#include "G330DepthWorkModeManager.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "DeviceComponentBase.hpp"
#include <map>

namespace libobsensor {

class G330SensorStreamStrategy : public ISensorStreamStrategy, public DeviceComponentBase {
public:
    G330SensorStreamStrategy(IDevice *owner);
    virtual ~G330SensorStreamStrategy() noexcept;

    void validateStream(const std::shared_ptr<const StreamProfile> &profile) override;
    void validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) override;
    void markStreamActivated(const std::shared_ptr<const StreamProfile> &profile) override;
    void markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile) override;

private:
    void validateDepthAndIrStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles);
    void validatePreset(const std::vector<std::shared_ptr<const StreamProfile>> &profiles);

private:
    std::mutex                                        startedStreamListMutex_;
    std::vector<std::shared_ptr<const StreamProfile>> activatedStreamList_;
};

}  // namespace libobsensor
