// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISensorStreamStrategy.hpp"
#include "stream/StreamProfile.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "DeviceComponentBase.hpp"
#include <map>

namespace libobsensor {

class AstraMiniSensorStreamStrategy : public ISensorStreamStrategy, public DeviceComponentBase {
public:
    AstraMiniSensorStreamStrategy(IDevice *owner);
    virtual ~AstraMiniSensorStreamStrategy() noexcept override;

    void validateStream(const std::shared_ptr<const StreamProfile> &profile) override;
    void validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) override;
    void markStreamActivated(const std::shared_ptr<const StreamProfile> &profile) override;
    void markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile) override;

private:


private:
    std::mutex                                        startedStreamListMutex_;
    std::vector<std::shared_ptr<const StreamProfile>> activatedStreamList_;
};

}  // namespace libobsensor
